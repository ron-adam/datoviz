#include "../include/datoviz/atlases.h"
#include "../include/datoviz/context.h"
#include "../include/datoviz/resources.h"
#include "font_atlas.h"
#include "resources_utils.h"



/*************************************************************************************************/
/*  Utils                                                                                    */
/*************************************************************************************************/

static DvzImages* _transfer_atlas(DvzContext* ctx)
{
    ASSERT(ctx != NULL);
    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    uvec3 shape = {256, 1, 1};
    VkDeviceSize size = 256 * sizeof(float);
    float* tex_data = (float*)calloc(256, sizeof(float));
    for (uint32_t i = 0; i < 256; i++)
        tex_data[i] = i / 255.0;

    DvzImages* img = _standalone_image(gpu, DVZ_TEX_1D, shape, VK_FORMAT_R32_SFLOAT);
    dvz_upload_image(&ctx->transfers, img, DVZ_ZERO_OFFSET, DVZ_ZERO_OFFSET, size, tex_data);

    FREE(tex_data);
    return img;
}



/*************************************************************************************************/
/*  Functions                                                                                    */
/*************************************************************************************************/

void dvz_atlases(DvzContext* ctx, DvzAtlases* atlases)
{
    ASSERT(ctx != NULL);
    DvzGpu* gpu = ctx->gpu;

    ASSERT(gpu != NULL);
    ASSERT(dvz_obj_is_created(&gpu->obj));

    ASSERT(atlases != NULL);
    ASSERT(!dvz_obj_is_created(&atlases->obj));
    // NOTE: this function should only be called once, at context creation.

    log_trace("creating atlases");

    atlases->ctx = ctx;

    // Font atlas.
    atlases->font_atlas = dvz_font_atlas(ctx);

    // Colormap atlas.
    DvzColormapAtlas* cmap_atlas = &atlases->cmap_atlas;
    cmap_atlas->arr = _load_colormaps();
    uvec3 shape = {256, 256, 1};
    cmap_atlas->img = _standalone_image(gpu, DVZ_TEX_2D, shape, VK_FORMAT_R8G8B8A8_UNORM);
    dvz_upload_image(
        &ctx->transfers, cmap_atlas->img, DVZ_ZERO_OFFSET, DVZ_ZERO_OFFSET, 256 * 256 * 4,
        cmap_atlas->arr);

    // Transfer atlas (1D texture), used for transfer functions.
    atlases->transfer_atlas = _transfer_atlas(ctx);

    dvz_obj_created(&atlases->obj);
}



void dvz_atlases_destroy(DvzAtlases* atlases)
{
    ASSERT(atlases != NULL);
    dvz_font_atlas_destroy(&atlases->font_atlas);

    _destroy_image(atlases->cmap_atlas.img);
    _destroy_image(atlases->transfer_atlas);
}
