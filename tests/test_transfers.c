#include "../include/datoviz/transfers.h"
#include "../src/resources_utils.h"
#include "../src/transfer_utils.h"
#include "proto.h"
#include "tests.h"



/*************************************************************************************************/
/*  Test callbacks and utils */
/*************************************************************************************************/

static void _dl_done(DvzDeq* deq, void* item, void* user_data)
{
    if (user_data != NULL)
        *((int*)user_data) = 42;
}



static DvzBufferRegions _mock_buffer(DvzGpu* gpu, VkDeviceSize size)
{
    ASSERT(gpu != NULL);
    DvzBuffer* buffer = (DvzBuffer*)calloc(1, sizeof(DvzBuffer));
    *buffer = dvz_buffer(gpu);
    _make_staging_buffer(buffer, size);
    DvzBufferRegions stg = dvz_buffer_regions(buffer, 1, 0, size, 0);
    return stg;
}

static void _destroy_buffer(DvzBufferRegions br)
{
    dvz_buffer_destroy(br.buffer);
    FREE(br.buffer);
}



static VkImageType _image_type_from_dims(DvzTexDims dims)
{
    switch (dims)
    {
    case DVZ_TEX_1D:
        return VK_IMAGE_TYPE_1D;
        break;
    case DVZ_TEX_2D:
        return VK_IMAGE_TYPE_2D;
        break;
    case DVZ_TEX_3D:
        return VK_IMAGE_TYPE_3D;
        break;

    default:
        break;
    }
    log_error("invalid image dimensions %d", dims);
    return VK_IMAGE_TYPE_2D;
}

static void _transition_image(DvzImages* img)
{
    ASSERT(img != NULL);

    DvzGpu* gpu = img->gpu;
    ASSERT(gpu != NULL);

    DvzCommands cmds_ = dvz_commands(gpu, 0, 1);
    DvzCommands* cmds = &cmds_;

    dvz_cmd_reset(cmds, 0);
    dvz_cmd_begin(cmds, 0);

    DvzBarrier barrier = dvz_barrier(gpu);
    dvz_barrier_stages(&barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    dvz_barrier_images(&barrier, img);
    dvz_barrier_images_layout(&barrier, VK_IMAGE_LAYOUT_UNDEFINED, img->layout);
    dvz_barrier_images_access(&barrier, 0, VK_ACCESS_TRANSFER_READ_BIT);
    dvz_cmd_barrier(cmds, 0, &barrier);

    dvz_cmd_end(cmds, 0);
    dvz_cmd_submit_sync(cmds, 0);
}

static DvzImages* _mock_image(DvzGpu* gpu, DvzTexDims dims, uvec3 shape, VkFormat format)
{
    ASSERT(gpu != NULL);
    ASSERT(1 <= dims && dims <= 3);
    log_debug(
        "creating %dD image with shape %dx%dx%d and format %d", //
        dims, shape[0], shape[1], shape[2], format);

    DvzImages* img = calloc(1, sizeof(DvzImages));

    *img = dvz_images(gpu, _image_type_from_dims(dims), 1);

    // Create the image.
    dvz_images_format(img, format);
    dvz_images_size(img, shape[0], shape[1], shape[2]);
    dvz_images_tiling(img, VK_IMAGE_TILING_OPTIMAL);
    dvz_images_layout(img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    dvz_images_usage(
        img,                                                      //
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | //
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    dvz_images_memory(img, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_images_queue_access(img, DVZ_DEFAULT_QUEUE_TRANSFER);
    dvz_images_queue_access(img, DVZ_DEFAULT_QUEUE_COMPUTE);
    dvz_images_queue_access(img, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_images_create(img);

    // Immediately transition the image to its layout.
    _transition_image(img);

    return img;
}

static void _destroy_image(DvzImages* img)
{
    ASSERT(img);
    dvz_images_destroy(img);
    FREE(img);
}



/*************************************************************************************************/
/*  Test enqueues/dequeues                                                                       */
/*************************************************************************************************/

int test_transfers_buffer_mappable(TestContext* tc)
{
    // ASSERT(tc->app != NULL);
    // DvzGpu* gpu = dvz_gpu_best(tc->app);
    // ASSERT(gpu != NULL);
    // dvz_gpu_default(gpu, NULL);
    // dvz_transfers(gpu, &tc->transfers);
    // dvz_transfers_destroy(&tc->transfers);
    // dvz_gpu_destroy(gpu);

    DvzTransfers* transfers = &tc->transfers;
    ASSERT(transfers != NULL);

    DvzGpu* gpu = transfers->gpu;
    ASSERT(gpu != NULL);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;

    // Allocate a staging buffer region.
    DvzBufferRegions stg = _mock_buffer(gpu, 1024);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&transfers->deq, stg, 0, (DvzBufferRegions){0}, 0, 128, data);

    // NOTE: need to wait for the upload to be finished before we download the data.
    // The DL and UL are on different queues and may be processed out of order.
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);

    // Enqueue a download transfer task.
    uint8_t data2[128] = {0};
    _enqueue_buffer_download(&transfers->deq, stg, 0, (DvzBufferRegions){0}, 0, 128, data2);
    AT(res == 0);
    dvz_deq_wait(&transfers->deq, DVZ_TRANSFER_PROC_UD);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);

    // Check that the copy worked.
    AT(data2[127] == 127);
    AT(memcmp(data2, data, 128) == 0);
    AT(res == 42);

    _destroy_buffer(stg);
    return 0;
}



int test_transfers_buffer_large(TestContext* tc)
{
    DvzTransfers* transfers = &tc->transfers;
    ASSERT(transfers != NULL);

    DvzGpu* gpu = transfers->gpu;
    ASSERT(gpu != NULL);

    uint64_t size = 32 * 1024 * 1024; // MB
    uint8_t* data = calloc(size, 1);
    data[0] = 1;
    data[size - 1] = 2;

    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    // Allocate a staging buffer region.
    DvzBufferRegions stg = _mock_buffer(gpu, size);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&transfers->deq, stg, 0, (DvzBufferRegions){0}, 0, size, data);

    // Wait for the transfer thread to process both transfer tasks.
    dvz_app_wait(tc->app);

    // Enqueue a download transfer task.
    uint8_t* data2 = calloc(size, 1);
    _enqueue_buffer_download(&transfers->deq, stg, 0, (DvzBufferRegions){0}, 0, size, data2);
    // This download task will be processed by the background transfer thread. At the end, it
    // will enqueue a DOWNLOAD_DONE task in the EV queue.
    AT(res == 0);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);

    // Check that the copy worked.
    AT(data2[0] == 1);
    AT(data2[size - 1] == 2);
    AT(memcmp(data2, data, size) == 0); // SHOULD FAIL
    AT(res == 42);

    FREE(data);
    FREE(data2);

    _destroy_buffer(stg);
    return 0;
}



int test_transfers_buffer_copy(TestContext* tc)
{
    DvzTransfers* transfers = &tc->transfers;
    ASSERT(transfers != NULL);

    DvzGpu* gpu = transfers->gpu;
    ASSERT(gpu != NULL);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;

    DvzBufferRegions stg = _mock_buffer(gpu, 1024);
    DvzBufferRegions br = _mock_buffer(gpu, 1024);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&transfers->deq, br, 0, stg, 0, 128, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Enqueue a download transfer task.
    uint8_t data2[128] = {0};
    _enqueue_buffer_download(&transfers->deq, br, 0, stg, 0, 128, data2);
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);

    dvz_app_wait(tc->app);

    // Check that the copy worked.
    AT(data2[127] == 127);
    AT(memcmp(data2, data, 128) == 0);
    AT(res == 42);

    dvz_buffer_destroy(stg.buffer);
    dvz_buffer_destroy(br.buffer);
    return 0;
}



int test_transfers_image_buffer(TestContext* tc)
{
    DvzTransfers* transfers = &tc->transfers;
    ASSERT(transfers != NULL);

    DvzGpu* gpu = transfers->gpu;
    ASSERT(gpu != NULL);

    uvec3 shape_full = {16, 48, 1};
    uvec3 offset = {0, 16, 0};
    uvec3 shape = {16, 16, 1};
    VkDeviceSize size = 256;
    VkFormat format = VK_FORMAT_R8G8B8A8_UINT;

    // Texture data.
    uint8_t data[256] = {0};
    for (uint32_t i = 0; i < 256; i++)
        data[i] = i;

    // Image.
    DvzImages* img = _mock_image(gpu, DVZ_TEX_2D, shape_full, format);
    // Buffer.
    DvzBufferRegions stg = _mock_buffer(gpu, size);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(
        &transfers->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    // Enqueue an upload transfer task.
    _enqueue_image_upload(&transfers->deq, img, offset, shape, stg, 0, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Enqueue a download transfer task.
    uint8_t data2[256] = {0};
    _enqueue_image_download(&transfers->deq, img, offset, shape, stg, 0, size, data2);
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);

    dvz_app_wait(tc->app);

    // Check.
    AT(memcmp(data2, data, 256) == 0);
    for (uint32_t i = 0; i < 256; i++)
        AT(data2[i] == i);

    _destroy_buffer(stg);
    _destroy_image(img);
    return 0;
}



/*************************************************************************************************/
/*  Test high-level transfer functions                                                           */
/*************************************************************************************************/

int test_transfers_direct_buffer(TestContext* tc)
{
    DvzTransfers* transfers = &tc->transfers;
    ASSERT(transfers != NULL);

    // // Create a data array.
    // uint8_t data[64] = {0};
    // for (uint32_t i = 0; i < 64; i++)
    //     data[i] = i;

    // VkDeviceSize offset = 32;
    // VkDeviceSize size = 64;

    // log_debug("start uploading data to buffer");

    // // Allocate a vertex buffer.
    // DvzBufferRegions br = dvz_ctx_buffers(transfers, DVZ_BUFFER_TYPE_VERTEX, 1, 128);
    // dvz_upload_buffer(transfers, br, offset, size, data);

    // log_debug("start downloading data from buffer");

    // // Enqueue a download transfer task.
    // uint8_t data2[64] = {0};
    // dvz_download_buffer(transfers, br, offset, size, data2);

    // // Check that the copy worked.
    // AT(memcmp(data2, data, size) == 0);

    return 0;
}



int test_transfers_direct_texture(TestContext* tc)
{
    DvzTransfers* transfers = &tc->transfers;
    ASSERT(transfers != NULL);

    // uvec3 shape_full = {16, 48, 1};
    // uvec3 offset = {0, 16, 0};
    // uvec3 shape = {16, 16, 1};
    // VkDeviceSize size = 256 * 4;
    // VkFormat format = VK_FORMAT_R8G8B8A8_UINT;

    // // Texture data.
    // uint8_t data[1024] = {0};
    // for (uint32_t i = 0; i < 1024; i++)
    //     data[i] = i % 256;

    // DvzTexture* tex = dvz_ctx_texture(transfers, 2, shape_full, format);

    // log_debug("start uploading data to texture");
    // dvz_upload_texture(transfers, tex, offset, shape, size, data);

    // log_debug("start downloading data from buffer");
    // uint8_t data2[1024] = {0};
    // dvz_download_texture(transfers, tex, offset, shape, size, data2);

    // // Check that the copy worked.
    // AT(memcmp(data2, data, size) == 0);

    return 0;
}
