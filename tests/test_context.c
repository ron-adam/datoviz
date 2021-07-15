#include "../include/datoviz/context.h"
#include "../src/transfer_utils.h"
#include "proto.h"
#include "tests.h"



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Buffer                                                                                       */
/*************************************************************************************************/

int test_context_buffer(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);
    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    // Allocate buffers.
    DvzBufferRegions br = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_UNIFORM_MAPPABLE, 1, 1024);
    VkDeviceSize offset = br.alignment;
    // DBG(br.alignment);
    // AT(br.aligned_size == 128);
    AT(br.count == 1);

    // Upload data.
    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;
    dvz_buffer_upload(br.buffer, offset, 32, data);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // Resize buffer.
    dvz_ctx_buffers_resize(ctx, &br, br.alignment);
    dvz_ctx_buffers_resize(ctx, &br, 1024 * 2);

    // Download data.
    uint8_t data_2[32] = {0};
    dvz_buffer_download(br.buffer, br.alignment, 32, data_2);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
    for (uint32_t i = 0; i < 32; i++)
        AT(data_2[i] == i);

    return 0;
}



int test_context_transfer_buffer(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    // Allocate buffers.
    DvzBufferRegions br = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STORAGE, 1, 128);
    AT(br.count == 1);

    // Upload data.
    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;

    // NOTE: when the app main loop is not running (which is the case here), these transfer
    // functions process the transfers immediately.
    dvz_upload_buffer(ctx, br, 64, 32, data);

    // Download data.
    uint8_t data_2[32] = {0};
    dvz_download_buffer(ctx, br, 64, 32, data_2);
    for (uint32_t i = 0; i < 32; i++)
        AT(data_2[i] == i);

    return 0;
}



/*************************************************************************************************/
/*  Compute                                                                                      */
/*************************************************************************************************/

int test_context_compute(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    char path[1024] = {0};
    snprintf(path, sizeof(path), "%s/test_double.comp.spv", SPIRV_DIR);
    dvz_ctx_compute(ctx, path);

    return 0;
}



/*************************************************************************************************/
/*  Texture                                                                                      */
/*************************************************************************************************/

int test_context_texture(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);
    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    uvec3 size = {16, 48, 1};
    uvec3 offset = {0, 16, 0};
    uvec3 shape = {16, 16, 1};
    VkFormat format = VK_FORMAT_R8G8B8A8_UINT;

    // Texture.
    DvzTexture* tex = dvz_ctx_texture(ctx, 2, size, format);
    dvz_texture_filter(tex, DVZ_FILTER_MAG, VK_FILTER_LINEAR);
    dvz_texture_address_mode(tex, DVZ_TEXTURE_AXIS_U, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // Texture data.
    uint8_t data[256] = {0};
    for (uint32_t i = 0; i < 256; i++)
        data[i] = i;
    dvz_texture_upload(tex, offset, shape, 256, data);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // Download data.
    uint8_t data_2[256] = {0};
    dvz_texture_download(tex, offset, shape, 256, data_2);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
    for (uint32_t i = 0; i < 256; i++)
        AT(data_2[i] == i);

    // Second texture.
    log_debug("copy texture");
    DvzTexture* tex_2 = dvz_ctx_texture(ctx, 2, shape, format);
    dvz_texture_copy(tex, offset, tex_2, DVZ_ZERO_OFFSET, shape);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // Download data.
    memset(data_2, 0, 256);
    dvz_texture_download(tex, offset, shape, 256, data_2);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
    for (uint32_t i = 0; i < 256; i++)
        AT(data_2[i] == i);

    // Resize the texture.
    // NOTE: for now, the texture data is LOST when resizing.
    size[1] = 64;
    dvz_texture_resize(tex, size);
    dvz_texture_download(tex, offset, shape, 256, data_2);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
    // for (uint32_t i = 0; i < 256; i++)
    //     AT(data_2[i] == i);

    return 0;
}



int test_context_transfer_texture(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);
    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    uvec3 size = {16, 48, 1};
    uvec3 offset = {0, 16, 0};
    uvec3 shape = {16, 16, 1};
    VkFormat format = VK_FORMAT_R8G8B8A8_UINT;

    // Texture.
    DvzTexture* tex = dvz_ctx_texture(ctx, 2, size, format);

    // Texture data.
    uint8_t data[256] = {0};
    for (uint32_t i = 0; i < 256; i++)
        data[i] = i;
    dvz_upload_texture(ctx, tex, offset, shape, 256, data);

    // Download data.
    uint8_t data_2[256] = {0};
    dvz_download_texture(ctx, tex, offset, shape, 256, data_2);
    for (uint32_t i = 0; i < 256; i++)
        AT(data_2[i] == i);

    return 0;
}



/*************************************************************************************************/
/*  Transfer dequeues                                                                            */
/*************************************************************************************************/

static void _dl_done(DvzDeq* deq, void* item, void* user_data)
{
    if (user_data != NULL)
        *((int*)user_data) = 42;
}

// static void _enqueue_buffer_download(
//     DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, DvzBufferRegions stg,
//     VkDeviceSize stg_offset, VkDeviceSize size, void* data)
// {
//     ASSERT(deq != NULL);
//     ASSERT(size > 0);

//     DvzTransfer* tr = calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
//     tr->type = DVZ_TRANSFER_BUFFER_DOWNLOAD;
//     tr->u.buf.br = br;
//     tr->u.buf.br_offset = br_offset;
//     tr->u.buf.stg = stg;
//     tr->u.buf.stg_offset = stg_offset;
//     tr->u.buf.size = size;
//     tr->u.buf.data = data;
//     dvz_deq_enqueue(deq, DVZ_CTX_DEQ_DL, tr->type, tr);
// }

int test_context_transfers_buffer_mappable(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(&ctx->deq, DVZ_CTX_DEQ_EV, DVZ_TRANSFER_BUFFER_DOWNLOAD_DONE, _dl_done, &res);

    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;

    // Allocate a staging buffer region.
    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, 1024);
    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&ctx->deq, (DvzBufferRegions){0}, 0, stg, 0, 128, data);

    // Enqueue a download transfer task.
    uint8_t data2[128] = {0};
    _enqueue_buffer_download(&ctx->deq, (DvzBufferRegions){0}, 0, stg, 0, 128, data2);
    AT(res == 0);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PEV, true);

    // Check that the copy worked.
    AT(data2[127] == 127);
    AT(memcmp(data2, data, 128) == 0);
    AT(res == 42);

    return 0;
}



int test_context_transfers_buffer_large(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    uint64_t size = 64 * 1024 * 1024;
    uint8_t* data = calloc(size, 1);
    data[0] = 1;
    data[size - 1] = 2;

    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(&ctx->deq, DVZ_CTX_DEQ_EV, DVZ_TRANSFER_BUFFER_DOWNLOAD_DONE, _dl_done, &res);

    // Allocate a staging buffer region.
    DvzBuffer* staging = (DvzBuffer*)dvz_container_get(&ctx->buffers, DVZ_BUFFER_TYPE_STAGING);
    dvz_buffer_resize(staging, size);
    DvzBufferRegions stg = dvz_buffer_regions(staging, 1, 0, size, 0);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&ctx->deq, (DvzBufferRegions){0}, 0, stg, 0, size, data);

    // Wait for the transfer thread to process both transfer tasks.
    dvz_app_wait(tc->app);

    // Enqueue a download transfer task.
    uint8_t* data2 = calloc(size, 1);
    _enqueue_buffer_download(&ctx->deq, (DvzBufferRegions){0}, 0, stg, 0, size, data2);
    // This download task will be processed by the background transfer thread. At the end, it will
    // enqueue a DOWNLOAD_DONE task in the EV queue.
    AT(res == 0);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PEV, true);

    // Check that the copy worked.
    AT(data2[0] == 1);
    AT(data2[size - 1] == 2);
    AT(memcmp(data2, data, size) == 0); // SHOULD FAIL
    AT(res == 42);

    FREE(data);
    FREE(data2);

    return 0;
}



int test_context_transfers_buffer_copy(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(&ctx->deq, DVZ_CTX_DEQ_EV, DVZ_TRANSFER_BUFFER_DOWNLOAD_DONE, _dl_done, &res);

    uint8_t data[128] = {0};
    for (uint32_t i = 0; i < 128; i++)
        data[i] = i;

    DvzBufferRegions stg = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_STAGING, 1, 1024);
    DvzBufferRegions br = dvz_ctx_buffers(ctx, DVZ_BUFFER_TYPE_VERTEX, 1, 1024);

    // Enqueue an upload transfer task.
    _enqueue_buffer_upload(&ctx->deq, br, 0, stg, 0, 128, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PCPY, true);

    // Enqueue a download transfer task.
    uint8_t data2[128] = {0};
    _enqueue_buffer_download(&ctx->deq, br, 0, stg, 0, 128, data2);
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PCPY, true);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PEV, true);

    dvz_app_wait(tc->app);

    // Check that the copy worked.
    AT(data2[127] == 127);
    AT(memcmp(data2, data, 128) == 0);
    AT(res == 42);

    return 0;
}



int test_context_transfers_texture(TestContext* tc)
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
    DvzTexture* stg = dvz_ctx_texture(ctx, 2, shape, format);

    // Callback for when the download has finished.
    int res = 0; // should be set to 42 by _dl_done().
    dvz_deq_callback(
        &ctx->deq, DVZ_CTX_DEQ_EV, DVZ_TRANSFER_TEXTURE_DOWNLOAD_DONE, _dl_done, &res);

    // Enqueue an upload transfer task.
    _enqueue_texture_upload(&ctx->deq, tex, offset, stg, (uvec3){0}, shape, size, data);
    // NOTE: we need to dequeue the copy proc manually, it is not done by the background thread
    // (the background thread only processes download/upload tasks).
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PCPY, true);

    // Enqueue a download transfer task.
    uint8_t data2[256] = {0};
    _enqueue_texture_download(&ctx->deq, tex, offset, stg, (uvec3){0}, shape, size, data2);
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PCPY, true);

    // Wait until the download_done event has been raised, dequeue it, and finish the test.
    dvz_deq_dequeue(&ctx->deq, DVZ_CTX_DEQ_PEV, true);

    dvz_app_wait(tc->app);

    // Check.
    AT(memcmp(data2, data, 256) == 0);
    for (uint32_t i = 0; i < 256; i++)
        AT(data2[i] == i);

    return 0;
}



/*************************************************************************************************/
/*  Colormap                                                                                     */
/*************************************************************************************************/

int test_context_colormap_custom(TestContext* tc)
{
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);
    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    // Make a custom colormap.
    uint8_t cmap = CMAP_CUSTOM;
    uint8_t color_count = 3;
    cvec4 colors[3] = {
        {255, 0, 0, 255},
        {0, 255, 0, 255},
        {0, 0, 255, 255},
    };

    // Update it on the CPU.
    dvz_colormap_custom(cmap, color_count, colors);

    // Check that the CPU array has been updated.
    cvec4 out = {0};
    for (uint32_t i = 0; i < 3; i++)
    {
        dvz_colormap(cmap, i, out);
        AT(memcmp(out, colors[i], sizeof(cvec4)) == 0);
    }

    // Update the colormap texture on the GPU.
    dvz_context_colormap(ctx);

    // Check that the GPU texture has been updated.
    cvec4* arr = calloc(256 * 256, sizeof(cvec4));
    dvz_texture_download(
        ctx->color_texture.texture, DVZ_ZERO_OFFSET, DVZ_ZERO_OFFSET, //
        256 * 256 * sizeof(cvec4), arr);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
    cvec2 ij = {0};
    dvz_colormap_idx(cmap, 0, ij);
    AT(memcmp(&arr[256 * ij[0] + ij[1]], colors, 3 * sizeof(cvec4)) == 0);
    FREE(arr);

    return 0;
}
