/*************************************************************************************************/
/*  Simple monospace font atlas                                                                  */
/*************************************************************************************************/

#ifndef DVZ_FONT_ATLAS_HEADER
#define DVZ_FONT_ATLAS_HEADER

#include "../include/datoviz/common.h"
#include "../include/datoviz/context.h"
#include "resources_utils.h"

#define STB_IMAGE_IMPLEMENTATION
BEGIN_INCL_NO_WARN
#include "../external/stb_image.h"
END_INCL_NO_WARN



/*************************************************************************************************/
/*  Font atlas                                                                                   */
/*************************************************************************************************/

static const char DVZ_FONT_ATLAS_STRING[] =
    " !\"#$%&'()*+,-./"
    "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7f";



static size_t _font_atlas_glyph(DvzFontAtlas* fa, const char* str, uint32_t idx)
{
    ASSERT(fa != NULL);
    ASSERT(fa->rows > 0);
    ASSERT(fa->cols > 0);
    ASSERT(str != NULL);
    ASSERT(strlen(str) > 0);
    ASSERT(idx < strlen(str));
    ASSERT(fa->font_str != NULL);
    ASSERT(strlen(fa->font_str) > 0);

    char c[2] = {str[idx], 0};
    return strcspn(fa->font_str, c);
}



static void _font_atlas_glyph_size(DvzFontAtlas* fa, float size, vec2 glyph_size)
{
    ASSERT(fa != NULL);
    glyph_size[0] = size * fa->glyph_width / fa->glyph_height;
    glyph_size[1] = size;
}



static DvzImages* _font_image(DvzContext* ctx, DvzFontAtlas* fa)
{
    ASSERT(ctx != NULL);

    DvzGpu* gpu = ctx->gpu;
    ASSERT(gpu != NULL);

    ASSERT(fa != NULL);
    ASSERT(fa->font_data != NULL);

    uvec3 shape = {(uint32_t)fa->width, (uint32_t)fa->height, 1};

    // NOTE: the font texture must have LINEAR filter! otherwise no antialiasing
    DvzImages* img = dvz_resources_image(&ctx->res, DVZ_TEX_2D, shape, VK_FORMAT_R8G8B8A8_UNORM);
    VkDeviceSize size = fa->width * fa->height * 4;
    dvz_upload_image(&ctx->transfers, img, DVZ_ZERO_OFFSET, DVZ_ZERO_OFFSET, size, fa->font_data);

    return img;
}



static DvzFontAtlas dvz_font_atlas(DvzContext* ctx)
{
    int width, height, depth;
    DvzFontAtlas font_atlas = {0};

    unsigned long file_size = 0;
    unsigned char* buffer = dvz_resource_font("font_inconsolata", &file_size);
    ASSERT(file_size > 0);
    ASSERT(buffer != NULL);

    font_atlas.font_data =
        stbi_load_from_memory(buffer, file_size, &width, &height, &depth, STBI_rgb_alpha);
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT(depth > 0);

    // TODO: parameters
    font_atlas.font_str = DVZ_FONT_ATLAS_STRING;
    ASSERT(strlen(font_atlas.font_str) > 0);
    font_atlas.cols = 16;
    font_atlas.rows = 6;

    font_atlas.width = (uint32_t)width;
    font_atlas.height = (uint32_t)height;
    font_atlas.glyph_width = font_atlas.width / (float)font_atlas.cols;
    font_atlas.glyph_height = font_atlas.height / (float)font_atlas.rows;

    font_atlas.img = _font_image(ctx, &font_atlas);

    return font_atlas;
}



static void dvz_font_atlas_destroy(DvzFontAtlas* fa)
{
    ASSERT(fa != NULL);
    ASSERT(fa->font_data != NULL);
    stbi_image_free(fa->font_data);

    // _destroy_image(fa->img);
}



#endif
