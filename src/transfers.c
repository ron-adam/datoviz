#include "../include/datoviz/transfers.h"
#include "../include/datoviz/canvas.h"
// #include "../include/datoviz/context.h"
#include "../include/datoviz/fifo.h"
// #include "context_utils.h"
#include "transfer_utils.h"



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

// Process for the deq proc #0, which encompasses the two queues UPLOAD and DOWNLOAD.
static void* _thread_transfers(void* user_data)
{
    DvzTransfers* transfers = (DvzTransfers*)user_data;
    ASSERT(transfers != NULL);
    dvz_deq_dequeue_loop(&transfers->deq, DVZ_TRANSFER_PROC_UD);
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
        DVZ_TRANSFER_IMAGE_COPY,                //
        _process_image_copy, transfers);

    // Buffer/image copies.
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_COPY, //
        DVZ_TRANSFER_IMAGE_BUFFER,              //
        _process_image_buffer, transfers);

    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_COPY, //
        DVZ_TRANSFER_BUFFER_IMAGE,              //
        _process_buffer_image, transfers);

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
    DvzTransfers* transfers, DvzBufferRegions br, //
    VkDeviceSize offset, VkDeviceSize size, void* data)
{
    ASSERT(transfers != NULL);
    ASSERT(br.buffer != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    log_debug("upload %s to a buffer", pretty_size(size));

    // TODO
    DvzBufferRegions stg = {0}; // dvz_ctx_buffers(transfers, DVZ_BUFFER_TYPE_STAGING, 1, size);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&transfers->deq, br, offset, stg, 0, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);
}



void dvz_download_buffer(
    DvzTransfers* transfers, DvzBufferRegions br, //
    VkDeviceSize offset, VkDeviceSize size, void* data)
{
    ASSERT(transfers != NULL);
    ASSERT(br.buffer != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    log_debug("download %s from a buffer", pretty_size(size));

    // TODO
    DvzBufferRegions stg = {0}; // dvz_ctx_buffers(transfers, DVZ_BUFFER_TYPE_STAGING, 1, size);

    // Enqueue an upload transfer task.
    _enqueue_buffer_download(&transfers->deq, br, offset, stg, 0, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);

    // Wait until the download is done.
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_EV);
}



void dvz_copy_buffer(
    DvzTransfers* transfers,                       //
    DvzBufferRegions src, VkDeviceSize src_offset, //
    DvzBufferRegions dst, VkDeviceSize dst_offset, //
    VkDeviceSize size)
{
    ASSERT(transfers != NULL);
    ASSERT(src.buffer != NULL);
    ASSERT(dst.buffer != NULL);
    ASSERT(size > 0);

    // Enqueue an upload transfer task.
    _enqueue_buffer_copy(&transfers->deq, src, src_offset, dst, dst_offset, size);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);
}



/*************************************************************************************************/
/*  Images transfers                                                                            */
/*************************************************************************************************/

static void _full_tex_shape(DvzImages* img, uvec3 shape)
{
    ASSERT(img != NULL);
    if (shape[0] == 0)
        shape[0] = img->width;
    if (shape[1] == 0)
        shape[1] = img->height;
    if (shape[2] == 0)
        shape[2] = img->depth;
}



void dvz_upload_image(
    DvzTransfers* transfers, DvzImages* img, //
    uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(transfers != NULL);
    ASSERT(img != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    _full_tex_shape(img, shape);
    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    // TODO
    DvzBufferRegions stg = {0}; // dvz_ctx_buffers(transfers, DVZ_BUFFER_TYPE_STAGING, 1, size);
    _enqueue_image_upload(&transfers->deq, img, offset, shape, stg, 0, size, data);

    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);
}



void dvz_download_image(
    DvzTransfers* transfers, DvzImages* img, //
    uvec3 offset, uvec3 shape, VkDeviceSize size, void* data)
{
    ASSERT(transfers != NULL);
    ASSERT(img != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    _full_tex_shape(img, shape);
    ASSERT(shape[0] > 0);
    ASSERT(shape[1] > 0);
    ASSERT(shape[2] > 0);

    // TODO
    DvzBufferRegions stg = {0}; // dvz_ctx_buffers(transfers, DVZ_BUFFER_TYPE_STAGING, 1, size);
    _enqueue_image_download(&transfers->deq, img, offset, shape, stg, 0, size, data);

    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);

    // Wait until the download is done.
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_EV);
}



void dvz_copy_image(
    DvzTransfers* transfers,          //
    DvzImages* src, uvec3 src_offset, //
    DvzImages* dst, uvec3 dst_offset, //
    uvec3 shape, VkDeviceSize size)
{
    _enqueue_image_copy(&transfers->deq, src, src_offset, dst, dst_offset, shape);

    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);
}
