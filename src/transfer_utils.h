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
/*  Buffer/Texture copy transfer task enqueuing                                                  */
/*************************************************************************************************/

static void _enqueue_texture_buffer(
    DvzDeq* deq,                                                     //
    DvzTexture* tex, uvec3 tex_offset, uvec3 shape,                  //
    DvzBufferRegions br, VkDeviceSize buf_offset, VkDeviceSize size, //
    void* to_download)
{
    ASSERT(deq != NULL);
    ASSERT(tex != NULL);
    ASSERT(br.buffer != NULL);
    log_trace("enqueue texture buffer copy");

    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    DvzTransferTextureBuffer* tr = (DvzTransferTextureBuffer*)calloc(
        1, sizeof(DvzTransferTextureBuffer)); // will be free-ed by the callbacks
    tr->tex = tex;
    memcpy(tr->tex_offset, tex_offset, sizeof(uvec3));
    memcpy(tr->shape, shape, sizeof(uvec3));
    tr->br = br;
    tr->buf_offset = buf_offset;
    tr->size = size;
    tr->to_download = to_download;
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_COPY, DVZ_TRANSFER_TEXTURE_BUFFER, tr);
}

static void _enqueue_buffer_texture(
    DvzDeq* deq,                                                     //
    DvzBufferRegions br, VkDeviceSize buf_offset, VkDeviceSize size, //
    DvzTexture* tex, uvec3 tex_offset, uvec3 shape                   //
)
{
    ASSERT(deq != NULL);
    ASSERT(tex != NULL);
    ASSERT(br.buffer != NULL);
    log_trace("enqueue buffer texture copy");

    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    DvzTransferBufferTexture* tr = (DvzTransferBufferTexture*)calloc(
        1, sizeof(DvzTransferBufferTexture)); // will be free-ed by the callbacks
    tr->br = br;
    tr->buf_offset = buf_offset;
    tr->size = size;
    tr->tex = tex;
    memcpy(tr->tex_offset, tex_offset, sizeof(uvec3));
    memcpy(tr->shape, shape, sizeof(uvec3));
    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_COPY, DVZ_TRANSFER_BUFFER_TEXTURE, tr);
}



/*************************************************************************************************/
/*  Buffer transfer task enqueuing                                                               */
/*************************************************************************************************/

static void _enqueue_buffer_upload(
    DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, //
    DvzBufferRegions stg, VkDeviceSize stg_offset,            //
    DvzTexture* tex, uvec3 tex_offset, uvec3 shape,           //
    VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);

    DvzTransferBuffer* tr = (DvzTransferBuffer*)calloc(1, sizeof(DvzTransferBuffer));

    tr->stg = stg;
    tr->stg_offset = stg_offset;
    tr->br = br;
    tr->br_offset = br_offset;
    tr->size = size;
    tr->data = data;

    // Copy the buffer to a texture after the data has been uploaded?
    tr->tex = tex;
    memcpy(tr->tex_offset, tex_offset, sizeof(uvec3));
    memcpy(tr->shape, shape, sizeof(uvec3));

    if (tex != NULL)
    {
        ASSERT(shape[0] > 0);
        ASSERT(shape[1] > 0);
        ASSERT(shape[2] > 0);
    }

    dvz_deq_enqueue(deq, DVZ_TRANSFER_DEQ_UL, DVZ_TRANSFER_BUFFER_UPLOAD, tr);
}

static void _enqueue_buffer_download(
    DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, DvzBufferRegions stg,
    VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);

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
    log_debug("process buffer upload");

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

    // Once the data has been transferred, enqueue a copy task from the staging buffer to the
    // texture.
    else if (tr->tex != NULL)
    {
        _enqueue_buffer_texture(
            deq, tr->stg, tr->stg_offset, tr->size, tr->tex, tr->tex_offset, tr->shape);
    }
}

static void _transfer_buffer_download(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransferBuffer* tr = (DvzTransferBuffer*)item;
    ASSERT(tr != NULL);
    log_debug("process buffer download");

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
    log_debug("process buffer copy");

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
    DvzDeq* deq, DvzTexture* tex, uvec3 offset, uvec3 shape, //
    DvzBufferRegions stg, VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(tex != NULL);
    ASSERT(data != NULL);
    ASSERT(stg.buffer != NULL);
    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    // Upload the data to the staging buffer.
    _enqueue_buffer_upload(
        deq, (DvzBufferRegions){0}, 0, stg, stg_offset, //
        // Let the task processor that once uploaded to the staging buffer, the data will have to
        // be copied to the texture.
        tex, offset, shape, size, data);
}

static void _enqueue_texture_download(
    DvzDeq* deq, DvzTexture* tex, uvec3 offset, uvec3 shape, //
    DvzBufferRegions stg, VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(tex != NULL);
    ASSERT(data != NULL);
    ASSERT(stg.buffer != NULL);
    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    // First, copy the texture to the staging buffer.
    _enqueue_texture_buffer(
        deq, tex, offset, shape, // texture
        stg, stg_offset, size,   // staging buffer
        data); // tell the processor that once copied to the buffer, the data will have to be
               // downloaded from the staging buffer
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

// static void _transfer_texture_upload(DvzDeq* deq, void* item, void* user_data)
// {
//     DvzTransferTexture* tr = (DvzTransferTexture*)item;
//     ASSERT(tr != NULL);
//     log_trace("process texture upload");

//     // Copy the data to the staging buffer.
//     ASSERT(tr->tex != NULL);
//     ASSERT(tr->size > 0);
//     ASSERT(tr->data != NULL);
//     ASSERT(tr->stg.buffer != NULL);

//     // Upload the data to the staging buffer.
//     _enqueue_buffer_upload(
//         deq, (DvzBufferRegions){0}, 0, tr->stg, tr->stg_offset, //
//         // Let the task processor that once uploaded to the staging buffer, the data will have
//         to
//         // be copied to the texture.
//         tr->tex, tr->offset, tr->shape, tr->size, tr->data);
// }

// static void _transfer_texture_download(DvzDeq* deq, void* item, void* user_data)
// {
//     DvzTransferTexture* tr = (DvzTransferTexture*)item;
//     ASSERT(tr != NULL);
//     log_trace("process texture download");

//     // Copy the data to the staging buffer.
//     ASSERT(tr->tex != NULL);
//     ASSERT(tr->size > 0);
//     ASSERT(tr->data != NULL);
//     ASSERT(tr->stg.buffer != NULL);

//     // First, copy the texture to the staging buffer.
//     _enqueue_texture_buffer(
//         deq, tr->tex, tr->offset, tr->shape, // texture
//         tr->stg, tr->stg_offset, tr->size,   // staging buffer
//         tr->data); // tell the processor that once copied to the buffer, the data will have to
//         be
//                    // downloaded from the staging buffer
// }

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
}



/*************************************************************************************************/
/*  Buffer/Texture copy transfer task processing                                                 */
/*************************************************************************************************/

static void _transfer_texture_buffer(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransferTextureBuffer* tr = (DvzTransferTextureBuffer*)item;
    ASSERT(tr != NULL);
    log_debug("process copy texture to buffer");

    // Copy the data to the staging buffer.
    ASSERT(tr->tex != NULL);
    ASSERT(tr->br.buffer != NULL);

    DvzContext* ctx = tr->tex->context;
    ASSERT(ctx != NULL);

    ASSERT(tr->shape[0] > 0);
    ASSERT(tr->shape[1] > 0);
    ASSERT(tr->shape[2] > 0);

    dvz_texture_copy_to_buffer(
        tr->tex, tr->tex_offset, tr->shape, tr->br, tr->buf_offset, tr->size);
    // Wait for the copy to be finished.
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);


    // If the buffer data has to be downloaded, enqueue the task.
    if (tr->to_download != NULL)
    {
        _enqueue_buffer_download(
            deq, (DvzBufferRegions){0}, 0, tr->br, tr->buf_offset, tr->size, tr->to_download);
    }
}

static void _transfer_buffer_texture(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransferBufferTexture* tr = (DvzTransferBufferTexture*)item;
    ASSERT(tr != NULL);
    log_debug("process copy buffer to texture");

    // Copy the data to the staging buffer.
    ASSERT(tr->tex != NULL);
    ASSERT(tr->br.buffer != NULL);

    DvzContext* ctx = tr->tex->context;
    ASSERT(ctx != NULL);

    ASSERT(tr->shape[0] > 0);
    ASSERT(tr->shape[1] > 0);
    ASSERT(tr->shape[2] > 0);

    dvz_texture_copy_from_buffer(
        tr->tex, tr->tex_offset, tr->shape, tr->br, tr->buf_offset, tr->size);
    // Wait for the copy to be finished.
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
}



#endif
