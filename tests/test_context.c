#include "../include/datoviz/context.h"
#include "../src/resources_utils.h"
#include "proto.h"
#include "tests.h"



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

int test_ctx_resources_1(TestContext* tc)
{
    ASSERT(tc != NULL);
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    DvzResources* res = &ctx->res;
    ASSERT(res != NULL);

    // Create some GPU objects, which should be automatically destroyed upon context destruction
    // (resources destruction).
    DvzBuffer* buffer = dvz_resources_buffer(res, DVZ_BUFFER_TYPE_VERTEX, false, 64);
    ASSERT(buffer != NULL);

    uvec3 shape = {2, 4, 1};
    DvzImages* img = dvz_resources_image(res, DVZ_TEX_2D, shape, VK_FORMAT_R8G8B8A8_UNORM);
    ASSERT(img != NULL);

    DvzSampler* sampler =
        dvz_resources_sampler(res, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    ASSERT(sampler != NULL);

    char path[1024];
    snprintf(path, sizeof(path), "%s/test_double.comp.spv", SPIRV_DIR);
    DvzCompute* compute = dvz_resources_compute(res, path);
    ASSERT(compute != NULL);

    return 0;
}



/*************************************************************************************************/
/*  Allocs                                                                                       */
/*************************************************************************************************/

int test_ctx_datalloc_1(TestContext* tc)
{
    ASSERT(tc != NULL);
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    VkDeviceSize alignment = 0;
    VkDeviceSize size = 128;

    // 2 allocations in the staging buffer.
    DvzDat* dat = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, size, 0);
    ASSERT(dat != NULL);
    AT(dat->br.offsets[0] == 0);
    AT(dat->br.size == size);

    // Get the buffer alignment.
    alignment = dat->br.buffer->vma.alignment;

    DvzDat* dat_1 = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, size, 0);
    ASSERT(dat_1 != NULL);
    AT(dat_1->br.offsets[0] == _align(size, alignment));
    AT(dat_1->br.size == size);

    // Resize the second buffer.
    VkDeviceSize new_size = 196;
    dvz_dat_resize(dat_1, new_size);
    // The offset should be the same, just the size should change.
    AT(dat_1->br.offsets[0] == _align(size, alignment));
    AT(dat_1->br.size == new_size);

    // 1 allocation in the vertex buffer.
    DvzDat* dat_2 = dvz_dat(ctx, DVZ_BUFFER_TYPE_VERTEX, size, 0);
    ASSERT(dat_2 != NULL);
    AT(dat_2->br.offsets[0] == 0);
    AT(dat_2->br.size == size);


    // Delete the first staging allocation.
    dvz_dat_destroy(dat);

    // New allocation in the staging buffer.
    DvzDat* dat_3 = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, size, 0);
    ASSERT(dat_3 != NULL);
    AT(dat_3->br.offsets[0] == 0);
    AT(dat_3->br.size == size);

    // Resize the lastly-created buffer, we should be in the first position.
    dvz_dat_resize(dat_3, new_size);
    AT(dat_3->br.offsets[0] == 0);
    AT(dat_3->br.size == new_size);

    // Resize the lastly-created buffer, we should now get a new position.
    new_size = 1024;
    dvz_dat_resize(dat_3, new_size);
    AT(dat_3->br.offsets[0] == 2 * alignment);
    AT(dat_3->br.size == new_size);


    return 0;
}



/*************************************************************************************************/
/*  Data transfers                                                                               */
/*************************************************************************************************/

int test_ctx_dat_1(TestContext* tc)
{
    ASSERT(tc != NULL);
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    uint32_t img_count = 3;
    dvz_context_img_count(ctx, img_count);

    // Allocate a dat.
    VkDeviceSize size = 128;
    uint8_t data[3] = {1, 2, 3};
    uint8_t data1[3] = {0};
    DvzDat* dat = NULL;

    int flags_tests[] = {
        DVZ_DAT_OPTIONS_NONE,       //
        DVZ_DAT_OPTIONS_STANDALONE, //
        DVZ_DAT_OPTIONS_MAPPABLE,   //
        DVZ_DAT_OPTIONS_DUP,
    };

    for (uint32_t i = 0; i < sizeof(flags_tests) / sizeof(int); i++)
    {
        // dat = dvz_dat(ctx, DVZ_BUFFER_TYPE_VERTEX, size, DVZ_DAT_OPTIONS_MAPPABLE);
        dat = dvz_dat(ctx, DVZ_BUFFER_TYPE_VERTEX, size, flags_tests[i]);
        ASSERT(dat != NULL);

        // Upload some data.
        dvz_dat_upload(dat, 0, sizeof(data), data, true);

        // Download back the data.
        dvz_dat_download(dat, 0, sizeof(data1), data1, true);
        // log_info("%d %d %d", data1[0], data1[1], data1[2]);
        AT(data1[0] == 1);
        AT(data1[1] == 2);
        AT(data1[2] == 3);

        dvz_dat_destroy(dat);
    }

    return 0;
}



int test_ctx_dat_resize(TestContext* tc)
{
    ASSERT(tc != NULL);
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    // Allocate a dat.
    VkDeviceSize size = 16;
    uint8_t data[16] = {0};
    for (uint32_t i = 0; i < size; i++)
        data[i] = i;
    uint8_t data1[32] = {0};

    // Upload.
    DvzDat* dat = dvz_dat(ctx, DVZ_BUFFER_TYPE_VERTEX, size, 0);
    dvz_dat_upload(dat, 0, sizeof(data), data, true);

    // Resize.
    VkDeviceSize new_size = 32;
    dvz_dat_resize(dat, new_size);

    // Download back the data.
    dvz_dat_download(dat, 0, sizeof(data1), data1, true);

    ASSERT(memcmp(data1, data, size) == 0);

    dvz_dat_destroy(dat);
    return 0;
}



int test_ctx_tex_1(TestContext* tc)
{
    ASSERT(tc != NULL);
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    uvec3 shape = {2, 4, 1};
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize size = 4 * shape[0] * shape[1] * shape[2];

    // Create a data array.
    uint8_t data[32] = {0};
    uint8_t data1[32] = {0};
    ASSERT(size == 32);
    for (uint32_t i = 0; i < size; i++)
        data[i] = i;

    // Allocate a tex.
    DvzTex* tex = dvz_tex(ctx, DVZ_TEX_2D, shape, format, 0);
    ASSERT(tex != NULL);

    // Upload some data.
    dvz_tex_upload(tex, DVZ_ZERO_OFFSET, shape, size, data, true);

    // Download back the data.
    dvz_tex_download(tex, DVZ_ZERO_OFFSET, shape, size, data1, true);

    for (uint32_t i = 0; i < size; i++)
        // log_info("%d", data1[i]);
        AT(data1[i] == i);

    dvz_tex_destroy(tex);

    return 0;
}



int test_ctx_tex_resize(TestContext* tc)
{
    ASSERT(tc != NULL);
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    uvec3 shape = {2, 4, 1};
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize size = 4 * shape[0] * shape[1] * shape[2];

    // Create a data array.
    uint8_t data[32] = {0};
    uint8_t data1[32 * 4] = {0};
    ASSERT(size == 32);
    for (uint32_t i = 0; i < size; i++)
        data[i] = i;

    // Allocate a tex.
    DvzTex* tex = dvz_tex(ctx, DVZ_TEX_2D, shape, format, 0);
    ASSERT(tex != NULL);

    // Upload some data.
    dvz_tex_upload(tex, DVZ_ZERO_OFFSET, shape, size, data, true);

    // Resize.
    uvec3 new_shape = {4, 8, 1};
    VkDeviceSize new_size = size * 4;
    dvz_tex_resize(tex, new_shape, new_size);

    // Download back the data.
    dvz_tex_download(tex, DVZ_ZERO_OFFSET, shape, size, data1, true);

    // NOTE: there is NO guarantee that the data will be kept upon resize. Whether that is the case
    // or not is undefined behavior. for (uint32_t i = 0; i < size; i++)
    //     AT(data1[i] == i);

    dvz_tex_destroy(tex);

    return 0;
}
