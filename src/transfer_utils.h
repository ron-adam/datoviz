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
    DvzTransfer* tr = (DvzTransfer*)calloc(1, sizeof(DvzTransfer));
    tr->type = DVZ_TRANSFER_BUFFER_UPLOAD;
    tr->u.buf.stg = stg;
    tr->u.buf.stg_offset = stg_offset;
    tr->u.buf.br = br;
    tr->u.buf.br_offset = br_offset;
    tr->u.buf.size = size;
    tr->u.buf.data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_UL, tr->type, tr);
}

static void _enqueue_buffer_download(
    DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, DvzBufferRegions stg,
    VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer download");

    DvzTransfer* tr =
        (DvzTransfer*)calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    tr->type = DVZ_TRANSFER_BUFFER_DOWNLOAD;
    tr->u.buf.br = br;
    tr->u.buf.br_offset = br_offset;
    tr->u.buf.stg = stg;
    tr->u.buf.stg_offset = stg_offset;
    tr->u.buf.size = size;
    tr->u.buf.data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_DL, tr->type, tr);
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

    DvzTransfer* trc =
        (DvzTransfer*)calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trc->type = DVZ_TRANSFER_BUFFER_COPY;
    trc->u.buf_copy.src = src;
    trc->u.buf_copy.src_offset = src_offset;
    trc->u.buf_copy.dst = dst;
    trc->u.buf_copy.dst_offset = dst_offset;
    trc->u.buf_copy.size = size;
    trc->u.buf_copy.to_download = to_download;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_COPY, trc->type, trc);
}

static void _enqueue_buffer_download_done(DvzDeq* deq, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer download done");

    DvzTransfer* tr =
        (DvzTransfer*)calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    tr->type = DVZ_TRANSFER_BUFFER_DOWNLOAD_DONE;
    tr->u.download.size = size;
    tr->u.download.data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_EV, tr->type, tr);
}



/*************************************************************************************************/
/*  Buffer transfer task processing                                                              */
/*************************************************************************************************/

static void _transfer_buffer_upload(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_BUFFER_UPLOAD);
    log_trace("process buffer upload");

    DvzTransferBuffer* trb = &tr->u.buf;

    // Copy the data to the staging buffer.
    ASSERT(trb->stg.buffer != NULL);
    ASSERT(trb->stg.size > 0);
    ASSERT(trb->size > 0);
    ASSERT(trb->stg_offset + trb->size <= trb->stg.size);

    // Take offset and size into account in the staging buffer.
    // NOTE: this call blocks while the data is being copied from CPU to GPU (mapped memcpy).
    dvz_buffer_regions_upload(&trb->stg, 0, trb->stg_offset, trb->size, trb->data);

    // Once the data has been transferred, enqueue a copy task from the staging buffer to the
    // destination buffer.
    if (trb->br.buffer != NULL)
    {
        _enqueue_buffer_copy(
            deq, trb->stg, trb->stg_offset, trb->br, trb->br_offset, trb->size, NULL);
    }
}

static void _transfer_buffer_download(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_BUFFER_DOWNLOAD);
    log_trace("process buffer download");

    DvzTransferBuffer* trb = &tr->u.buf;

    // Copy the data to the staging buffer.
    ASSERT(trb->stg.buffer != NULL);
    ASSERT(trb->stg.size > 0);
    ASSERT(trb->size > 0);
    ASSERT(trb->stg_offset + trb->size <= trb->stg.size);

    // If the data is to be downloaded from a non-mappable buffer, we need to enqueue a copy and
    // stop here the download callback. We'll specify, in the copy task, that the download must be
    // enqueued afterwards.
    if (trb->br.buffer != NULL)
    {
        _enqueue_buffer_copy(
            deq, trb->br, trb->br_offset, trb->stg, trb->stg_offset, trb->size, trb->data);
    }

    // If the data is to be downloaded from a mappable (staging) buffer, we can download the data
    // directly here (blocking call).
    else
    {
        // Take offset and size into account in the staging buffer.
        // NOTE: this call blocks while the data is being copied from GPU to CPU (mapped memcpy).
        dvz_buffer_regions_download(&trb->stg, 0, trb->stg_offset, trb->size, trb->data);

        // Raise a DOWNLOAD_DONE event when the download has finished.
        _enqueue_buffer_download_done(deq, trb->size, trb->data);
    }
}

static void _transfer_buffer_copy(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(user_data != NULL);
    DvzContext* ctx = (DvzContext*)user_data;
    log_trace("process buffer copy");

    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_BUFFER_COPY);

    DvzTransferBufferCopy* trb = &tr->u.buf_copy;

    // Make the GPU-GPU buffer copy (block the GPU and wait for the copy to finish).
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_buffer_regions_copy(&trb->src, trb->src_offset, &trb->dst, trb->dst_offset, trb->size);
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // If needed, enqueue a download task after the copy. The destination buffer of the copy then
    // needs to be a mappable buffer (typically, a staging buffer).
    if (trb->to_download != NULL)
    {
        _enqueue_buffer_download(
            deq, (DvzBufferRegions){0}, 0, trb->dst, trb->dst_offset, trb->size, trb->to_download);
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
    DvzDeq* deq, DvzTexture* tex, uvec3 tex_offset, DvzTexture* stg, uvec3 stg_offset, uvec3 shape,
    VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    DvzTransfer* tr = (DvzTransfer*)calloc(1, sizeof(DvzTransfer));
    tr->type = DVZ_TRANSFER_TEXTURE_UPLOAD;
    tr->u.tex.stg = stg;
    tr->u.tex.tex = tex;
    memcpy(tr->u.tex.stg_offset, stg_offset, sizeof(uvec3));
    memcpy(tr->u.tex.tex_offset, tex_offset, sizeof(uvec3));
    memcpy(tr->u.tex.shape, shape, sizeof(uvec3));
    tr->u.tex.size = size;
    tr->u.tex.data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_UL, tr->type, tr);
}

static void _enqueue_texture_download(
    DvzDeq* deq, DvzTexture* tex, uvec3 tex_offset, DvzTexture* stg, uvec3 stg_offset, uvec3 shape,
    VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(stg != NULL);
    log_trace("enqueue texture download");

    DvzTransfer* tr =
        (DvzTransfer*)calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    tr->type = DVZ_TRANSFER_TEXTURE_DOWNLOAD;
    tr->u.tex.tex = tex;
    tr->u.tex.stg = stg;
    memcpy(tr->u.tex.stg_offset, stg_offset, sizeof(uvec3));
    memcpy(tr->u.tex.tex_offset, tex_offset, sizeof(uvec3));
    memcpy(tr->u.tex.shape, shape, sizeof(uvec3));
    tr->u.tex.size = size;
    tr->u.tex.data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_DL, tr->type, tr);
}

static void _enqueue_texture_copy(
    DvzDeq* deq, DvzTexture* src, uvec3 src_offset, DvzTexture* dst, uvec3 dst_offset, uvec3 shape,
    VkDeviceSize size, void* to_download)
{
    ASSERT(deq != NULL);
    ASSERT(src != NULL);
    ASSERT(dst != NULL);
    log_trace("enqueue texture copy");

    DvzTransfer* trc =
        (DvzTransfer*)calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trc->type = DVZ_TRANSFER_TEXTURE_COPY;
    trc->u.tex_copy.src = src;
    trc->u.tex_copy.dst = dst;
    trc->u.tex_copy.size = size;
    trc->u.tex_copy.to_download = to_download;
    memcpy(trc->u.tex_copy.src_offset, src_offset, sizeof(uvec3));
    memcpy(trc->u.tex_copy.dst_offset, dst_offset, sizeof(uvec3));
    memcpy(trc->u.tex_copy.shape, shape, sizeof(uvec3));
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_COPY, trc->type, trc);
}

static void _enqueue_texture_download_done(DvzDeq* deq, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue texture download done");

    DvzTransfer* tr =
        (DvzTransfer*)calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    tr->type = DVZ_TRANSFER_TEXTURE_DOWNLOAD_DONE;
    tr->u.download.size = size;
    tr->u.download.data = data;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_EV, tr->type, tr);
}



/*************************************************************************************************/
/*  Texture transfer task processing                                                             */
/*************************************************************************************************/

static void _transfer_texture_upload(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_TEXTURE_UPLOAD);
    log_trace("process texture upload");

    DvzTransferTexture* trt = &tr->u.tex;

    // Copy the data to the staging buffer.
    ASSERT(trt->stg != NULL);
    ASSERT(trt->size > 0);

    // Take offset and size into account in the staging buffer.
    // NOTE: this call blocks while the data is being copied from CPU to GPU (mapped memcpy).
    dvz_texture_upload(trt->stg, trt->stg_offset, trt->shape, trt->size, trt->data);

    // Once the data has been transferred, enqueue a copy task from the staging buffer to the
    // destination buffer.
    if (trt->tex != NULL)
    {
        _enqueue_texture_copy(
            deq, trt->stg, trt->stg_offset, trt->tex, trt->tex_offset, trt->shape, trt->size,
            NULL);
    }
}

static void _transfer_texture_download(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_TEXTURE_DOWNLOAD);
    log_trace("process texture download");

    DvzTransferTexture* trt = &tr->u.tex;

    // Copy the data to the staging buffer.
    ASSERT(trt->stg != NULL);
    ASSERT(trt->size > 0);

    // If the data is to be downloaded from a non-mappable buffer, we need to enqueue a copy and
    // stop here the download callback. We'll specify, in the copy task, that the download must be
    // enqueued afterwards.
    if (trt->tex != NULL)
    {
        _enqueue_texture_copy(
            deq, trt->tex, trt->tex_offset, trt->stg, trt->stg_offset, trt->shape, trt->size,
            trt->data);
    }

    // If the data is to be downloaded from a mappable (staging) buffer, we can download the data
    // directly here (blocking call).
    else
    {
        // Take offset and size into account in the staging buffer.
        // NOTE: this call blocks while the data is being copied from GPU to CPU (mapped memcpy).
        dvz_texture_download(trt->stg, trt->stg_offset, trt->shape, trt->size, trt->data);

        // Raise a DOWNLOAD_DONE event when the download has finished.
        _enqueue_texture_download_done(deq, trt->size, trt->data);
    }
}

static void _transfer_texture_copy(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(user_data != NULL);
    DvzContext* ctx = (DvzContext*)user_data;
    log_trace("process texture copy");

    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_TEXTURE_COPY);

    DvzTransferTextureCopy* trt = &tr->u.tex_copy;

    // Make the GPU-GPU buffer copy (block the GPU and wait for the copy to finish).
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_texture_copy(trt->src, trt->src_offset, trt->dst, trt->dst_offset, trt->shape);
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // If needed, enqueue a download task after the copy. The destination buffer of the copy then
    // needs to be a mappable buffer (typically, a staging buffer).
    if (trt->to_download != NULL)
    {
        _enqueue_texture_download(
            deq, NULL, trt->src_offset, trt->dst, trt->dst_offset, trt->shape, trt->size,
            trt->to_download);
    }
}



#endif
