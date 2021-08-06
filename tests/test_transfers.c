#include "../include/datoviz/context.h"
#include "../src/transfer_utils.h"
#include "proto.h"
#include "tests.h"



/*************************************************************************************************/
/*  Test enqueues/dequeues                                                                       */
/*************************************************************************************************/

static void _dl_done(DvzDeq* deq, void* item, void* user_data)
{
    if (user_data != NULL)
        *((int*)user_data) = 42;
}



int test_transfers_buffer_mappable(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(&ctx->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;

    // Allocate a staging buffer region.
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, 1024);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&ctx->deq, stg, 0, (DvzBufferRegions){0}, 0, 128, data);

    // NOTE: need to wait for the upload to be finished before we download the data.
    // The DL and UL are on different queues and may be processed out of order.
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);

    // Enqueue a download transfer task.
    uint8_t data2[128] = {0};
    _enqueue_buffer_download(&ctx->deq, stg, 0, (DvzBufferRegions){0}, 0, 128, data2);
    AT(res == 0);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_EV, true);

    // Check that the copy worked.
    AT(data2[127] == 127);
    AT(memcmp(data2, data, 128) == 0);
    AT(res == 42);

    return 0;
}



int test_transfers_buffer_large(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    uint64_t size = 32 * 1024 * 1024; // MB
    uint8_t* data = calloc(size, 1);
    data[0] = 1;
    data[size - 1] = 2;

    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(&ctx->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    // Allocate a staging buffer region.
    DvzBuffers* staging = (DvzBuffers*)dvz_container_get(&ctx->buffers, DVZ_BUFFER_TYPE_STAGING);
    dvz_buffer_resize(staging, size);
    DvzBufferRegions stg = dvz_buffer_regions(staging, 1, 0, size, 0);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&ctx->deq, stg, 0, (DvzBufferRegions){0}, 0, size, data);

    // Wait for the transfer thread to process both transfer tasks.
    dvz_app_wait(tc->app);

    // Enqueue a download transfer task.
    uint8_t* data2 = calloc(size, 1);
    _enqueue_buffer_download(&ctx->deq, stg, 0, (DvzBufferRegions){0}, 0, size, data2);
    // This download task will be processed by the background transfer thread. At the end, it will
    // enqueue a DOWNLOAD_DONE task in the EV queue.
    AT(res == 0);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_EV, true);

    // Check that the copy worked.
    AT(data2[0] == 1);
    AT(data2[size - 1] == 2);
    AT(memcmp(data2, data, size) == 0); // SHOULD FAIL
    AT(res == 42);

    FREE(data);
    FREE(data2);

    return 0;
}



int test_transfers_buffer_copy(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(&ctx->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;

    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, 1024);
    DvzBufferRegions br = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_VERTEX, 1, 1024);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&ctx->deq, br, 0, stg, 0, 128, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Enqueue a download transfer task.
    uint8_t data2[128] = {0};
    _enqueue_buffer_download(&ctx->deq, br, 0, stg, 0, 128, data2);
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_EV, true);

    dvz_app_wait(tc->app);

    // Check that the copy worked.
    AT(data2[127] == 127);
    AT(memcmp(data2, data, 128) == 0);
    AT(res == 42);

    return 0;
}



int test_transfers_texture_buffer(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    uvec3 shape_full = {16, 48, 1};
    uvec3 offset = {0, 16, 0};
    uvec3 shape = {16, 16, 1};
    VkDeviceSize size = 256;
    VkFormat format = VK_FORMAT_R8G8B8A8_UINT;

    // Texture data.
    uint8_t data[256] = {0};
    for (uint32_t i = 0; i < 256; i++)
        data[i] = i;

    // Texture.
    DvzTexture* tex = dvz_ctx_texture(ctx, 2, shape_full, format);
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(&ctx->deq, DVZ_TRANSFER_DEQ_EV, DVZ_TRANSFER_DOWNLOAD_DONE, _dl_done, &res);

    // Enqueue an upload transfer task.
    _enqueue_texture_upload(&ctx->deq, tex, offset, shape, stg, 0, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Enqueue a download transfer task.
    uint8_t data2[256] = {0};
    _enqueue_texture_download(&ctx->deq, tex, offset, shape, stg, 0, size, data2);
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_EV, true);

    dvz_app_wait(tc->app);

    // Check.
    AT(memcmp(data2, data, 256) == 0);
    for (uint32_t i = 0; i < 256; i++)
        AT(data2[i] == i);

    return 0;
}



/*************************************************************************************************/
/*  Test high-level transfer functions                                                           */
/*************************************************************************************************/

int test_transfers_direct_buffer(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    // Create a data array.
    uint8_t data[64] = {0};
    for (uint32_t i = 0; i < 64; i++)
        data[i] = i;

    VkDeviceSize offset = 32;
    VkDeviceSize size = 64;

    log_debug("start uploading data to buffer");

    // Allocate a vertex buffer.
    DvzBufferRegions br = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_VERTEX, 1, 128);
    dvz_upload_buffer(ctx, br, offset, size, data);

    log_debug("start downloading data from buffer");

    // Enqueue a download transfer task.
    uint8_t data2[64] = {0};
    dvz_download_buffer(ctx, br, offset, size, data2);

    // Check that the copy worked.
    AT(memcmp(data2, data, size) == 0);

    return 0;
}



int test_transfers_direct_texture(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    uvec3 shape_full = {16, 48, 1};
    uvec3 offset = {0, 16, 0};
    uvec3 shape = {16, 16, 1};
    VkDeviceSize size = 256 * 4;
    VkFormat format = VK_FORMAT_R8G8B8A8_UINT;

    // Texture data.
    uint8_t data[1024] = {0};
    for (uint32_t i = 0; i < 1024; i++)
        data[i] = i % 256;

    DvzTexture* tex = dvz_ctx_texture(ctx, 2, shape_full, format);

    log_debug("start uploading data to texture");
    dvz_upload_texture(ctx, tex, offset, shape, size, data);

    log_debug("start downloading data from buffer");
    uint8_t data2[1024] = {0};
    dvz_download_texture(ctx, tex, offset, shape, size, data2);

    // Check that the copy worked.
    AT(memcmp(data2, data, size) == 0);

    return 0;
}
