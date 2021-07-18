#include "../include/datoviz/transfers.h"
#include "../include/datoviz/canvas.h"
#include "../include/datoviz/context.h"
#include "../include/datoviz/fifo.h"
#include "context_utils.h"
#include "transfer_utils.h"



// NOTE WARNING: PUBLIC FUNCTIONS HERE NOT TESTED YET
// These convenient functions use the Dequeue interface. They are not efficient.



/*************************************************************************************************/
/*  FIFO                                                                                         */
/*************************************************************************************************/

// static void _transfer_enqueue(DvzFifo* fifo, DvzTransfer transfer)
// {
//     ASSERT(fifo->capacity > 0);
//     ASSERT(0 <= fifo->tail && fifo->tail < fifo->capacity);
//     DvzTransfer* tr = (DvzTransfer*)calloc(1, sizeof(DvzTransfer));
//     *tr = transfer;
//     dvz_fifo_enqueue(fifo, tr);
// }



// static DvzTransfer _transfer_dequeue(DvzFifo* fifo, bool wait)
// {
//     DvzTransfer* item = (DvzTransfer*)dvz_fifo_dequeue(fifo, wait);
//     DvzTransfer out;
//     out.type = DVZ_TRANSFER_NONE;
//     if (item == NULL)
//         return out;
//     ASSERT(item != NULL);
//     out = *item;
//     FREE(item);
//     return out;
// }



/*************************************************************************************************/
/*  Buffer transfers                                                                             */
/*************************************************************************************************/

// static void _process_buffer_upload(DvzContext* ctx, DvzTransfer tr)
// {
//     ASSERT(context != NULL);
//     DvzGpu* gpu = context->gpu;
//     ASSERT(gpu != NULL);
//     ASSERT(tr.type == DVZ_TRANSFER_BUFFER_UPLOAD);

//     DvzBufferRegions br = tr.u.buf.br;
//     ASSERT(br.size > 0);
//     ASSERT(br.count == 1);
//     ASSERT(tr.u.buf.data != NULL);
//     ASSERT(tr.u.buf.size > 0);
//     ASSERT(tr.u.buf.br.buffer != VK_NULL_HANDLE);
//     ASSERT(
//         br.buffer->type != DVZ_BUFFER_TYPE_STAGING &&
//         br.buffer->type != DVZ_BUFFER_TYPE_UNIFORM_MAPPABLE);

//     // Take the staging buffer and ensure it is big enough.
//     DvzBuffer* staging = staging_buffer(context, tr.u.buf.size);

//     // Memcpy into the staging buffer.
//     dvz_buffer_upload(staging, 0, tr.u.buf.size, tr.u.buf.data);

//     // Copy from the staging buffer to the target buffer.
//     _copy_buffer_from_staging(context, tr.u.buf.br, tr.u.buf.stg_offset, tr.u.buf.size);

//     // IMPORTANT: need to wait for the texture to be copied to the staging buffer, *before*
//     // downloading the data from the staging buffer.
//     dvz_queue_wait(context->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
// }



// static void _process_buffer_download(DvzContext* ctx, DvzTransfer tr)
// {
//     ASSERT(context != NULL);
//     ASSERT(tr.type == DVZ_TRANSFER_BUFFER_DOWNLOAD);

//     DvzBufferRegions br = tr.u.buf.br;
//     ASSERT(br.size > 0);
//     ASSERT(br.count == 1);
//     ASSERT(tr.u.buf.data != NULL);
//     ASSERT(tr.u.buf.size > 0);
//     ASSERT(tr.u.buf.br.buffer != VK_NULL_HANDLE);
//     ASSERT(
//         br.buffer->type != DVZ_BUFFER_TYPE_STAGING &&
//         br.buffer->type != DVZ_BUFFER_TYPE_UNIFORM_MAPPABLE);

//     // Take the staging buffer and ensure it is big enough.
//     DvzBuffer* staging = staging_buffer(context, tr.u.buf.size);

//     // Copy from the source buffer to the staging buffer.
//     _copy_buffer_to_staging(context, tr.u.buf.br, tr.u.buf.stg_offset, tr.u.buf.size);

//     // IMPORTANT: need to wait for the texture to be copied to the staging buffer, *before*
//     // downloading the data from the staging buffer.
//     dvz_queue_wait(context->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

//     // Memcpy into the staging buffer.
//     dvz_buffer_download(staging, 0, tr.u.buf.size, tr.u.buf.data);
// }



// static void _process_buffer_copy(DvzContext* ctx, DvzTransfer tr)
// {
//     ASSERT(context != NULL);
//     ASSERT(tr.type == DVZ_TRANSFER_BUFFER_COPY);

//     DvzBufferRegions* src = &tr.u.buf_copy.src;
//     DvzBufferRegions* dst = &tr.u.buf_copy.dst;
//     ASSERT(src->count == dst->count);

//     VkDeviceSize size = tr.u.buf_copy.size;
//     VkDeviceSize src_offset = tr.u.buf_copy.src_offset;
//     VkDeviceSize dst_offset = tr.u.buf_copy.dst_offset;

//     dvz_buffer_regions_copy(src, src_offset, dst, dst_offset, size);
// }



/*************************************************************************************************/
/*  Texture transfers                                                                            */
/*************************************************************************************************/

// static void _process_texture_upload(DvzContext* ctx, DvzTransfer tr)
// {
//     ASSERT(context != NULL);
//     ASSERT(tr.type == DVZ_TRANSFER_TEXTURE_UPLOAD);

//     dvz_texture_upload(
//         tr.u.tex.tex, tr.u.tex.tex_offset, tr.u.tex.shape, tr.u.tex.size, tr.u.tex.data);
// }



// static void _process_texture_download(DvzContext* ctx, DvzTransfer tr)
// {
//     ASSERT(context != NULL);
//     ASSERT(tr.type == DVZ_TRANSFER_TEXTURE_DOWNLOAD);

//     dvz_texture_download(
//         tr.u.tex.tex, tr.u.tex.tex_offset, tr.u.tex.shape, tr.u.tex.size, tr.u.tex.data);
// }



// static void _process_texture_copy(DvzContext* ctx, DvzTransfer tr)
// {
//     ASSERT(context != NULL);
//     ASSERT(tr.type == DVZ_TRANSFER_TEXTURE_COPY);

//     dvz_texture_copy(
//         tr.u.tex_copy.src, tr.u.tex_copy.src_offset, tr.u.tex_copy.dst,
//         tr.u.tex_copy.dst_offset, tr.u.tex_copy.shape);
// }



/*************************************************************************************************/
/*  Canvas transfers processing                                                                  */
/*************************************************************************************************/

// void dvz_process_transfers(DvzContext* context)
// {
//     // WARNING: comment below OBSOLETE.

//     // This function is to be called at every frame, after the FRAME callbacks (so that FRAME
//     // callbacks calling dvz_upload_buffer() have their transfers processed immediately in the
//     // same frame), but before queue submit, so that we may get a chance to ask for a command
//     // buffer refill before submission (if a transfer requires a refill, e.g. after a vertex
//     buffer
//     // count change)

//     ASSERT(context != NULL);
//     DvzGpu* gpu = context->gpu;
//     ASSERT(gpu != NULL);

//     DvzFifo* fifo = &context->transfers;
//     // Do nothing if there are no pending transfers.
//     if (fifo->is_empty)
//         return;

//     // NOTE: wait until all render tasks have finished.
//     dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_RENDER);

//     // Process all pending transfer tasks.
//     DvzTransfer tr = {0};
//     while (true)
//     {
//         tr = _transfer_dequeue(fifo, false);
//         if (tr.type == DVZ_TRANSFER_NONE)
//             break;
//         fifo->is_processing = true;

//         // Process buffer transfers.
//         if (tr.type == DVZ_TRANSFER_BUFFER_UPLOAD)
//             _process_buffer_upload(context, tr);
//         if (tr.type == DVZ_TRANSFER_BUFFER_DOWNLOAD)
//             _process_buffer_download(context, tr);
//         if (tr.type == DVZ_TRANSFER_BUFFER_COPY)
//             _process_buffer_copy(context, tr);

//         // Process texture transfers.
//         if (tr.type == DVZ_TRANSFER_TEXTURE_UPLOAD)
//             _process_texture_upload(context, tr);
//         if (tr.type == DVZ_TRANSFER_TEXTURE_DOWNLOAD)
//             _process_texture_download(context, tr);
//         if (tr.type == DVZ_TRANSFER_TEXTURE_COPY)
//             _process_texture_copy(context, tr);

//         fifo->is_processing = false;
//     }

//     // NOTE: wait until all transfer tasks have finished.
//     dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
// }



/*************************************************************************************************/
/*  Canvas buffer transfers                                                                      */
/*************************************************************************************************/

// static void _enqueue_buffer_transfer(
//     DvzContext* ctx, DvzDataTransferType type, DvzBufferRegions br, //
//     VkDeviceSize offset, VkDeviceSize size, void* data)
// {
//     ASSERT(context != NULL);
//     ASSERT(context->gpu != NULL);
//     ASSERT(context->transfers.capacity > 0);
//     ASSERT(size > 0);
//     ASSERT(br.buffer != NULL);
//     ASSERT(dvz_obj_is_created(&br.buffer->obj));
//     ASSERT(data != NULL);

//     // Create the transfer object.
//     DvzTransfer tr = {0};
//     tr.type = type;
//     tr.u.buf.br = br;
//     tr.u.buf.stg_offset = offset;
//     tr.u.buf.size = size;
//     tr.u.buf.data = data;

//     _transfer_enqueue(&context->transfers, tr);
// }



// WARNING: these functions require that the pointer lives through the next frame (no copy)
void dvz_upload_buffer(
    DvzContext* ctx, DvzBufferRegions br, VkDeviceSize offset, VkDeviceSize size, void* data)
{
    ASSERT(ctx != NULL);
    ASSERT(br.buffer != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    // TODO: better staging buffer allocation
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&ctx->deq, br, offset, stg, 0, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);
}



void dvz_download_buffer(
    DvzContext* ctx, DvzBufferRegions br, VkDeviceSize offset, VkDeviceSize size, void* data)
{
    ASSERT(ctx != NULL);
    ASSERT(br.buffer != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    // TODO: better staging buffer allocation
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size);

    // Enqueue an upload transfer task.
    _enqueue_buffer_download(&ctx->deq, br, offset, stg, 0, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);
}



void dvz_copy_buffer(
    DvzContext* ctx, DvzBufferRegions src, VkDeviceSize src_offset, //
    DvzBufferRegions dst, VkDeviceSize dst_offset, VkDeviceSize size)
{
    // TODO

    // ASSERT(context != NULL);
    // ASSERT(context->gpu != NULL);
    // ASSERT(context->gpu->app != NULL);
    // ASSERT(context->transfers.capacity > 0);

    // ASSERT(size > 0);
    // ASSERT(src.buffer != NULL);
    // ASSERT(dst.buffer != NULL);

    // // Create the transfer object.
    // DvzTransfer tr = {0};
    // tr.type = DVZ_TRANSFER_BUFFER_COPY;
    // tr.u.buf_copy.src = src;
    // tr.u.buf_copy.dst = dst;
    // tr.u.buf_copy.src_offset = src_offset;
    // tr.u.buf_copy.dst_offset = dst_offset;
    // tr.u.buf_copy.size = size;

    // _transfer_enqueue(&context->transfers, tr);

    // if (!context->gpu->app->is_running)
    //     dvz_process_transfers(context);
}



/*************************************************************************************************/
/*  Canvas texture transfers                                                                     */
/*************************************************************************************************/

// static void _enqueue_texture_transfer(
//     DvzContext* ctx, DvzDataTransferType type, DvzTexture* texture, //
//     uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
// {
//     ASSERT(context != NULL);
//     ASSERT(context->gpu != NULL);
//     ASSERT(context->transfers.capacity > 0);

//     ASSERT(texture != NULL);
//     ASSERT(dvz_obj_is_created(&texture->obj));
//     ASSERT(size > 0);
//     ASSERT(data != NULL);

//     if (shape[0] == 0)
//         shape[0] = texture->image->width;
//     if (shape[1] == 0)
//         shape[1] = texture->image->height;
//     if (shape[2] == 0)
//         shape[2] = texture->image->depth;

//     // Create the transfer object.
//     DvzTransfer tr = {0};
//     tr.type = type;
//     for (uint32_t i = 0; i < 3; i++)
//     {
//         tr.u.tex.shape[i] = shape[i];
//         tr.u.tex.tex_offset[i] = offset[i];
//     }
//     tr.u.tex.size = size;
//     tr.u.tex.data = data;
//     tr.u.tex.tex = texture;

//     _transfer_enqueue(&context->transfers, tr);
// }



static uint32_t _get_texture_ndims(DvzTexture* tex)
{
    if (tex->image->image_type == VK_IMAGE_TYPE_1D)
        return 1;
    else if (tex->image->image_type == VK_IMAGE_TYPE_2D)
        return 2;
    else if (tex->image->image_type == VK_IMAGE_TYPE_3D)
        return 3;
    return 0;
}



void dvz_upload_texture(
    DvzContext* ctx, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(ctx != NULL);
    ASSERT(tex != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    // DvzTexture* stg = dvz_ctx_texture(ctx, _get_texture_ndims(tex), shape, tex->image->format);
    _enqueue_texture_upload(&ctx->deq, tex, offset, shape, size, data);

    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);

    // dvz_texture_destroy(stg);
}



void dvz_download_texture(
    DvzContext* ctx, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(ctx != NULL);
    ASSERT(tex != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    // DvzTexture* stg = dvz_ctx_texture(ctx, _get_texture_ndims(tex), shape, tex->image->format);
    _enqueue_texture_download(&ctx->deq, tex, offset, shape, size, data);

    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);

    // dvz_texture_destroy(stg);
}



void dvz_copy_texture(
    DvzContext* ctx, DvzTexture* src, uvec3 src_offset, DvzTexture* dst, uvec3 dst_offset,
    uvec3 shape, VkDeviceSize size)
{
    // TODO
    // ASSERT(context != NULL);
    // ASSERT(context->gpu != NULL);
    // ASSERT(context->transfers.capacity > 0);

    // ASSERT(src != NULL);
    // ASSERT(dvz_obj_is_created(&src->obj));
    // ASSERT(dst != NULL);
    // ASSERT(dvz_obj_is_created(&dst->obj));
    // ASSERT(size > 0);

    // // Create the transfer object.
    // DvzTransfer tr = {0};
    // tr.type = DVZ_TRANSFER_TEXTURE_COPY;
    // tr.u.tex_copy.src = src;
    // tr.u.tex_copy.dst = dst;
    // memcpy(tr.u.tex_copy.src_offset, src_offset, sizeof(uvec3));
    // memcpy(tr.u.tex_copy.dst_offset, dst_offset, sizeof(uvec3));
    // memcpy(tr.u.tex_copy.shape, shape, sizeof(uvec3));

    // _transfer_enqueue(&context->transfers, tr);

    // if (!context->gpu->app->is_running)
    //     dvz_process_transfers(context);
}
