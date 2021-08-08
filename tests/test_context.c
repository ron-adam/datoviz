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

int test_ctx_allocs_1(TestContext* tc)
{
    ASSERT(tc != NULL);
    DvzContext* ctx = tc->context;
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    VkDeviceSize size = 256;
    DvzDat* dat = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, size, 1, 0);
    ASSERT(dat != NULL);
    AT(dat->br.offsets[0] == 0);
    AT(dat->br.size == size);

    DvzDat* dat_1 = dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, size, 1, 0);
    ASSERT(dat_1 != NULL);
    AT(dat_1->br.offsets[0] == 256);
    AT(dat_1->br.size == size);



    return 0;
}
