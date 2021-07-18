/*************************************************************************************************/
/* Data transfer                                                                                 */
/*************************************************************************************************/

#ifndef DVZ_TRANSFERS_UTILS_HEADER
#define DVZ_TRANSFERS_UTILS_HEADER

#include "../include/datoviz/context.h"
#include "../include/datoviz/transfers.h"
#include "../include/datoviz/vklite.h"



/*************************************************************************************************/
/*  New transfers                                                                                */
/*************************************************************************************************/

// Process for the deq proc #0, which encompasses the two queues UPLOAD and DOWNLOAD.
static void* _thread_transfers(void* user_data)
{
    DvzContext* ctx = (DvzContext*)user_data;
    ASSERT(ctx != NULL);
    dvz_deq_dequeue_loop(&ctx->deq, DVZ_TRANSFER_PROC_UD);
    return NULL;
}



/*************************************************************************************************/
/*  Buffer transfer task enqueuing                                                               */
/*************************************************************************************************/

static void _enqueue_buffer_upload(
    DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, DvzBufferRegions stg,
    VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    DvzTransferBuffer* tr = (DvzTransferBuffer*)calloc(1, sizeof(DvzTransferBuffer));
    tr->stg = stg;
    tr->stg_offset = stg_offset;
    tr->br = br;
    tr->br_offset = br_offset;
    tr->size = size;
    tr->data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_UL, DVZ_TRANSFER_BUFFER_UPLOAD, tr);
}

static void _enqueue_buffer_download(
    DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, DvzBufferRegions stg,
    VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer download");

    DvzTransferBuffer* tr = (DvzTransferBuffer*)calloc(
        1, sizeof(DvzTransferBuffer)); // will be free-ed by the callbacks
    tr->br = br;
    tr->br_offset = br_offset;
    tr->stg = stg;
    tr->stg_offset = stg_offset;
    tr->size = size;
    tr->data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_DL, DVZ_TRANSFER_BUFFER_DOWNLOAD, tr);
}

static void _enqueue_buffer_copy(
    DvzDeq* deq, DvzBufferRegions src, VkDeviceSize src_offset, DvzBufferRegions dst,
    VkDeviceSize dst_offset, VkDeviceSize size, void* to_download)
{
    ASSERT(deq != NULL);
    ASSERT(src.buffer != NULL);
    ASSERT(dst.buffer != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer copy");

    DvzTransferBufferCopy* tr = (DvzTransferBufferCopy*)calloc(
        1, sizeof(DvzTransferBufferCopy)); // will be free-ed by the callbacks
    tr->src = src;
    tr->src_offset = src_offset;
    tr->dst = dst;
    tr->dst_offset = dst_offset;
    tr->size = size;
    tr->to_download = to_download;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_COPY, DVZ_TRANSFER_BUFFER_COPY, tr);
}

static void _enqueue_buffer_download_done(DvzDeq* deq, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer download done");

    DvzTransferDownload* tr = (DvzTransferDownload*)calloc(
        1, sizeof(DvzTransferDownload)); // will be free-ed by the callbacks
    tr->size = size;
    tr->data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_BUFFER_DOWNLOAD_DONE, tr);
}



/*************************************************************************************************/
/*  Buffer transfer task processing                                                              */
/*************************************************************************************************/

static void _transfer_buffer_upload(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransferBuffer* tr = (DvzTransferBuffer*)item;
    ASSERT(tr != NULL);
    log_trace("process buffer upload");

    // Copy the data to the staging buffer.
    ASSERT(tr->stg.buffer != NULL);
    ASSERT(tr->stg.size > 0);
    ASSERT(tr->size > 0);
    ASSERT(tr->stg_offset + tr->size <= tr->stg.size);

    // Take offset and size into account in the staging buffer.
    // NOTE: this call blocks while the data is being copied from CPU to GPU (mapped memcpy).
    dvz_buffer_regions_upload(&tr->stg, 0, tr->stg_offset, tr->size, tr->data);

    // Once the data has been transferred, enqueue a copy task from the staging buffer to the
    // destination buffer.
    if (tr->br.buffer != NULL)
    {
        _enqueue_buffer_copy(deq, tr->stg, tr->stg_offset, tr->br, tr->br_offset, tr->size, NULL);
    }
}

static void _transfer_buffer_download(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransferBuffer* tr = (DvzTransferBuffer*)item;
    ASSERT(tr != NULL);
    log_trace("process buffer download");

    // Copy the data to the staging buffer.
    ASSERT(tr->stg.buffer != NULL);
    ASSERT(tr->stg.size > 0);
    ASSERT(tr->size > 0);
    ASSERT(tr->stg_offset + tr->size <= tr->stg.size);

    // If the data is to be downloaded from a non-mappable buffer, we need to enqueue a copy and
    // stop here the download callback. We'll specify, in the copy task, that the download must be
    // enqueued afterwards.
    if (tr->br.buffer != NULL)
    {
        _enqueue_buffer_copy(
            deq, tr->br, tr->br_offset, tr->stg, tr->stg_offset, tr->size, tr->data);
    }

    // If the data is to be downloaded from a mappable (staging) buffer, we can download the data
    // directly here (blocking call).
    else
    {
        // Take offset and size into account in the staging buffer.
        // NOTE: this call blocks while the data is being copied from GPU to CPU (mapped memcpy).
        dvz_buffer_regions_download(&tr->stg, 0, tr->stg_offset, tr->size, tr->data);

        // Raise a DOWNLOAD_DONE event when the download has finished.
        _enqueue_buffer_download_done(deq, tr->size, tr->data);
    }
}

static void _transfer_buffer_copy(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(user_data != NULL);
    DvzContext* ctx = (DvzContext*)user_data;
    log_trace("process buffer copy");

    DvzTransferBufferCopy* tr = (DvzTransferBufferCopy*)item;
    ASSERT(tr != NULL);

    // Make the GPU-GPU buffer copy (block the GPU and wait for the copy to finish).
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_buffer_regions_copy(&tr->src, tr->src_offset, &tr->dst, tr->dst_offset, tr->size);
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // If needed, enqueue a download task after the copy. The destination buffer of the copy then
    // needs to be a mappable buffer (typically, a staging buffer).
    if (tr->to_download != NULL)
    {
        _enqueue_buffer_download(
            deq, (DvzBufferRegions){0}, 0, tr->dst, tr->dst_offset, tr->size, tr->to_download);
    }
}



static void _texture_shape(DvzTexture* texture, uvec3 shape)
{
    ASSERT(texture != NULL);
    ASSERT(texture->image != NULL);

    if (shape[0] == 0)
        shape[0] = texture->image->width;
    if (shape[1] == 0)
        shape[1] = texture->image->height;
    if (shape[2] == 0)
        shape[2] = texture->image->depth;
}



/*************************************************************************************************/
/*  Texture transfer task enqueuing                                                              */
/*************************************************************************************************/

static void _enqueue_texture_upload(
    DvzDeq* deq, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    DvzTransferTexture* tr = (DvzTransferTexture*)calloc(1, sizeof(DvzTransferTexture));
    // tr->stg = stg;
    tr->tex = tex;
    // memcpy(tr->stg_offset, stg_offset, sizeof(uvec3));
    // memcpy(tr->tex_offset, tex_offset, sizeof(uvec3));
    memcpy(tr->offset, offset, sizeof(uvec3));
    memcpy(tr->shape, shape, sizeof(uvec3));
    tr->size = size;
    tr->data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_UL, DVZ_TRANSFER_TEXTURE_UPLOAD, tr);
}

static void _enqueue_texture_download(
    DvzDeq* deq, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    // ASSERT(stg != NULL);
    log_trace("enqueue texture download");

    DvzTransferTexture* tr = (DvzTransferTexture*)calloc(
        1, sizeof(DvzTransferTexture)); // will be free-ed by the callbacks
    tr->tex = tex;
    // tr->stg = stg;
    // memcpy(tr->stg_offset, stg_offset, sizeof(uvec3));
    // memcpy(tr->tex_offset, tex_offset, sizeof(uvec3));
    memcpy(tr->offset, offset, sizeof(uvec3));
    memcpy(tr->shape, shape, sizeof(uvec3));
    tr->size = size;
    tr->data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_DL, DVZ_TRANSFER_TEXTURE_DOWNLOAD, tr);
}

static void _enqueue_texture_copy(
    DvzDeq* deq, DvzTexture* src, uvec3 src_offset, DvzTexture* dst, uvec3 dst_offset, uvec3 shape,
    VkDeviceSize size) //, void* to_download)
{
    ASSERT(deq != NULL);
    ASSERT(src != NULL);
    ASSERT(dst != NULL);
    log_trace("enqueue texture copy");

    DvzTransferTextureCopy* tr = (DvzTransferTextureCopy*)calloc(
        1, sizeof(DvzTransferTextureCopy)); // will be free-ed by the callbacks
    tr->src = src;
    tr->dst = dst;
    tr->size = size;
    // tr->to_download = to_download;
    memcpy(tr->src_offset, src_offset, sizeof(uvec3));
    memcpy(tr->dst_offset, dst_offset, sizeof(uvec3));
    memcpy(tr->shape, shape, sizeof(uvec3));
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_COPY, DVZ_TRANSFER_TEXTURE_COPY, tr);
}

static void _enqueue_texture_download_done(DvzDeq* deq, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue texture download done");

    DvzTransferDownload* tr = (DvzTransferDownload*)calloc(
        1, sizeof(DvzTransferDownload)); // will be free-ed by the callbacks
    tr->size = size;
    tr->data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_TEXTURE_DOWNLOAD_DONE, tr);
}



/*************************************************************************************************/
/*  Texture transfer task processing                                                             */
/*************************************************************************************************/

static void _transfer_texture_upload(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransferTexture* tr = (DvzTransferTexture*)item;
    ASSERT(tr != NULL);
    log_trace("process texture upload");

    // Copy the data to the staging buffer.
    // ASSERT(tr->stg != NULL);
    ASSERT(tr->size > 0);

    // Take offset and size into account in the staging buffer.
    // NOTE: this call blocks while the data is being copied from CPU to GPU (mapped memcpy).

    // TODO OPTIM: cut the transfer in 2, first copy from/to a GPU buffer, then make the
    // buffer-texture copy with hard sync.
    dvz_texture_upload(tr->tex, tr->offset, tr->shape, tr->size, tr->data);

    // // Once the data has been transferred, enqueue a copy task from the staging buffer to the
    // // destination buffer.
    // if (tr->tex != NULL)
    // {
    //     _enqueue_texture_copy(
    //         deq, tr->stg, tr->stg_offset, tr->tex, tr->tex_offset, tr->shape, tr->size, NULL);
    // }
}

static void _transfer_texture_download(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransferTexture* tr = (DvzTransferTexture*)item;
    ASSERT(tr != NULL);
    log_trace("process texture download");

    // Copy the data to the staging buffer.
    ASSERT(tr->tex != NULL);
    ASSERT(tr->size > 0);

    // If the data is to be downloaded from a non-mappable buffer, we need to enqueue a copy and
    // stop here the download callback. We'll specify, in the copy task, that the download must be
    // enqueued afterwards.
    // if (tr->tex != NULL)
    // {
    //     _enqueue_texture_copy(
    //         deq, tr->tex, tr->tex_offset, tr->stg, tr->stg_offset, tr->shape, tr->size,
    //         tr->data);
    // }

    // If the data is to be downloaded from a mappable (staging) buffer, we can download the data
    // directly here (blocking call).
    // else
    // {
    // Take offset and size into account in the staging buffer.
    // NOTE: this call blocks while the data is being copied from GPU to CPU (mapped memcpy).
    dvz_texture_download(tr->tex, tr->offset, tr->shape, tr->size, tr->data);

    // Raise a DOWNLOAD_DONE event when the download has finished.
    _enqueue_texture_download_done(deq, tr->size, tr->data);
    // }
}

static void _transfer_texture_copy(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(user_data != NULL);
    DvzContext* ctx = (DvzContext*)user_data;
    log_trace("process texture copy");

    DvzTransferTextureCopy* tr = (DvzTransferTextureCopy*)item;
    ASSERT(tr != NULL);

    // Make the GPU-GPU buffer copy (block the GPU and wait for the copy to finish).
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_texture_copy(tr->src, tr->src_offset, tr->dst, tr->dst_offset, tr->shape);
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // // If needed, enqueue a download task after the copy. The destination buffer of the copy
    // then
    // // needs to be a mappable buffer (typically, a staging buffer).
    // if (tr->to_download != NULL)
    // {
    //     _enqueue_texture_download(
    //         deq, tr->dst, tr->dst_offset, tr->shape, tr->size, tr->to_download);
    // }
}



#endif
