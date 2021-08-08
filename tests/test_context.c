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
    DvzBuffer* buffer = dvz_resources_buffer(res, DVZ_BUFFER_TYPE_VERTEX, 64);
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
    DvzDat* dat = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size, 0);
    ASSERT(dat != NULL);
    AT(dat->br.offsets[0] == 0);
    AT(dat->br.size == size);

    // Get the buffer alignment.
    alignment = dat->br.buffer->vma.alignment;

    DvzDat* dat_1 = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size, 0);
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
    DvzDat* dat_2 = dvz_dat(ctx, DVZ_BUFFER_TYPE_VERTEX, 1, size, 0);
    ASSERT(dat_2 != NULL);
    AT(dat_2->br.offsets[0] == 0);
    AT(dat_2->br.size == size);


    // Delete the first staging allocation.
    dvz_dat_destroy(dat);

    // New allocation in the staging buffer.
    DvzDat* dat_3 = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, 1, size, 0);
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
