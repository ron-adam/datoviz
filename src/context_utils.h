#ifndef DVZ_CONTEXT_UTILS_HEADER
#define DVZ_CONTEXT_UTILS_HEADER

#include "../include/datoviz/context.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Staging buffer                                                                               */
/*************************************************************************************************/

// TODO: remove:
// Get the staging buffer, and make sure it can contain `size` bytes.
static DvzBuffer* staging_buffer(DvzContext* ctx, VkDeviceSize size)
{
    // log_trace("requesting staging buffer of size %s", pretty_size(size));
    // DvzBuffer* staging = (DvzBuffer*)dvz_container_get(&ctx->buffers, DVZ_BUFFER_TYPE_STAGING);
    // ASSERT(staging != NULL);
    // ASSERT(staging->buffer != VK_NULL_HANDLE);

    // // Resize the staging buffer is needed.
    // // TODO: keep staging buffer fixed and copy parts of the data to staging buffer in several
    // // steps?
    // if (staging->size < size)
    // {
    //     VkDeviceSize new_size = dvz_next_pow2(size);
    //     log_debug("reallocating staging buffer to %s", pretty_size(new_size));
    //     dvz_buffer_resize(staging, new_size);
    // }
    // ASSERT(staging->size >= size);
    // return staging;
    return NULL;
}



/*************************************************************************************************/
/*  Default resources                                                                            */
/*************************************************************************************************/

static DvzTex* _default_transfer_texture(DvzContext* ctx)
{
    // // TODO
    // ASSERT(ctx != NULL);
    // DvzGpu* gpu = ctx->gpu;
    // ASSERT(gpu != NULL);

    // uvec3 shape = {256, 1, 1};
    // DvzTex* texture = dvz_ctx_texture(ctx, 1, shape, VK_FORMAT_R32_SFLOAT);
    // float* tex_data = (float*)calloc(256, sizeof(float));
    // for (uint32_t i = 0; i < 256; i++)
    //     tex_data[i] = i / 255.0;
    // dvz_texture_address_mode(texture, DVZ_TEXTURE_AXIS_U,
    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE); uvec3 offset = {0, 0, 0};

    // // dvz_upload_texture(context, texture, offset, shape, 256 * sizeof(float), tex_data);
    // // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // FREE(tex_data);
    // return texture;
    return NULL;
}



#ifdef __cplusplus
}
#endif

#endif
