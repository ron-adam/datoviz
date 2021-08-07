#include "../include/datoviz/transfers.h"
#include "../include/datoviz/canvas.h"
#include "../include/datoviz/context.h"
#include "../include/datoviz/fifo.h"
#include "context_utils.h"
#include "transfer_utils.h"



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

// Process for the deq proc #0, which encompasses the two queues UPLOAD and DOWNLOAD.
static void* _thread_transfers(void* user_data)
{
    DvzContext* ctx = (DvzContext*)user_data;
    ASSERT(ctx != NULL);
    dvz_deq_dequeue_loop(&ctx->deq, DVZ_TRANSFER_PROC_UD);
    return NULL;
}



static void _create_transfers(DvzTransfers* transfers)
{
    ASSERT(transfers != NULL);
    transfers->deq = dvz_deq(4);

    // Three producer/consumer pairs (deq processes).
    dvz_deq_proc(
        &transfers->deq, DVZ_TRANSFER_PROC_UD, //
        2, (uint32_t[]){DVZ_TRANSFER_DEQ_UL, DVZ_TRANSFER_DEQ_DL});
    dvz_deq_proc(
        &transfers->deq, DVZ_TRANSFER_PROC_CPY, //
        1, (uint32_t[]){DVZ_TRANSFER_DEQ_COPY});
    dvz_deq_proc(
        &transfers->deq, DVZ_TRANSFER_PROC_EV, //
        1, (uint32_t[]){DVZ_TRANSFER_DEQ_EV});

    // Transfer deq callbacks.
    // Uploads.
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_UL, //
        DVZ_TRANSFER_BUFFER_UPLOAD,           //
        _process_buffer_upload, transfers);

    // Downloads.
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_DL, //
        DVZ_TRANSFER_BUFFER_DOWNLOAD,         //
        _process_buffer_download, transfers);

    // Copies.
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_COPY, //
        DVZ_TRANSFER_BUFFER_COPY,               //
        _process_buffer_copy, transfers);

    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_COPY, //
        DVZ_TRANSFER_TEXTURE_COPY,              //
        _process_texture_copy, transfers);

    // Buffer/texture copies.
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_COPY, //
        DVZ_TRANSFER_TEXTURE_BUFFER,            //
        _process_texture_buffer, transfers);

    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_COPY, //
        DVZ_TRANSFER_BUFFER_TEXTURE,            //
        _process_buffer_texture, transfers);

    // Transfer thread.
    transfers->thread = dvz_thread(_thread_transfers, transfers);
}



/*************************************************************************************************/
/*  Transfers struct                                                                             */
/*************************************************************************************************/

DvzTransfers* dvz_transfers(DvzGpu* gpu)
{
    ASSERT(gpu != NULL);
    ASSERT(dvz_obj_is_created(&gpu->obj));
    log_trace("creating transfers");

    DvzTransfers* transfers = calloc(1, sizeof(DvzTransfers));
    ASSERT(transfers != NULL);

    // Create the transfers.
    transfers->gpu = gpu;
    _create_transfers(transfers);
    dvz_obj_created(&transfers->obj);

    gpu->transfers = transfers;
    return transfers;
}



void dvz_transfers_destroy(DvzTransfers* transfers)
{
    ASSERT(transfers != NULL);

    // Enqueue a STOP task to stop the UL and DL threads.
    dvz_deq_enqueue(&transfers->deq, DVZ_TRANSFER_DEQ_UL, 0, NULL);
    dvz_deq_enqueue(&transfers->deq, DVZ_TRANSFER_DEQ_DL, 0, NULL);

    // Join the UL and DL threads.
    dvz_thread_join(&transfers->thread);

    dvz_deq_destroy(&transfers->deq);
}



// WARNING: the functions below are convenient because they return immediately, but they are not
// optimally efficient because of the use of hard GPU synchronization primitives.

/*************************************************************************************************/
/*  Buffer transfers                                                                             */
/*************************************************************************************************/

void dvz_upload_buffer(
    DvzContext* ctx, DvzBufferRegions br, VkDeviceSize offset, VkDeviceSize size, void* data)
{
    ASSERT(ctx != NULL);
    ASSERT(br.buffer != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    log_debug("upload %s to a buffer", pretty_size(size));

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

    log_debug("download %s from a buffer", pretty_size(size));

    // TODO: better staging buffer allocation
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size);

    // Enqueue an upload transfer task.
    _enqueue_buffer_download(&ctx->deq, br, offset, stg, 0, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);

    // Wait until the download is done.
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_EV, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_EV);
}



void dvz_copy_buffer(
    DvzContext* ctx, DvzBufferRegions src, VkDeviceSize src_offset, //
    DvzBufferRegions dst, VkDeviceSize dst_offset, VkDeviceSize size)
{
    ASSERT(ctx != NULL);
    ASSERT(src.buffer != NULL);
    ASSERT(dst.buffer != NULL);
    ASSERT(size > 0);

    // Enqueue an upload transfer task.
    _enqueue_buffer_copy(&ctx->deq, src, src_offset, dst, dst_offset, size);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);
}



/*************************************************************************************************/
/*  Texture transfers                                                                            */
/*************************************************************************************************/

static void _full_tex_shape(DvzTexture* tex, uvec3 shape)
{
    ASSERT(tex != NULL);
    if (shape[0] == 0)
        shape[0] = tex->image->width;
    if (shape[1] == 0)
        shape[1] = tex->image->height;
    if (shape[2] == 0)
        shape[2] = tex->image->depth;
}



void dvz_upload_texture(
    DvzContext* ctx, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(ctx != NULL);
    ASSERT(tex != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    _full_tex_shape(tex, shape);
    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    // TODO: better alloc, mark unused alloc as done
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size);
    _enqueue_texture_upload(&ctx->deq, tex, offset, shape, stg, 0, size, data);

    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);
}



void dvz_download_texture(
    DvzContext* ctx, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(ctx != NULL);
    ASSERT(tex != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    _full_tex_shape(tex, shape);
    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    // TODO: better alloc, mark unused alloc as done
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size);
    _enqueue_texture_download(&ctx->deq, tex, offset, shape, stg, 0, size, data);

    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);

    // Wait until the download is done.
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_EV, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_EV);
}



void dvz_copy_texture(
    DvzContext* ctx, DvzTexture* src, uvec3 src_offset, DvzTexture* dst, uvec3 dst_offset,
    uvec3 shape, VkDeviceSize size)
{
    _enqueue_texture_copy(&ctx->deq, src, src_offset, dst, dst_offset, shape);

    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);
}
