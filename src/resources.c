#include "../include/datoviz/resources.h"
#include "resources_utils.h"
#include "vklite_utils.h"
#include <stdlib.h>



/*************************************************************************************************/
/*  Resource utils                                                                               */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

void dvz_resources(DvzGpu* gpu, DvzResources* res)
{
    ASSERT(gpu != NULL);
    ASSERT(dvz_obj_is_created(&gpu->obj));
    ASSERT(res != NULL);
    ASSERT(!dvz_obj_is_created(&res->obj));
    // NOTE: this function should only be called once, at context creation.

    log_trace("creating resources");

    // Create the resources.
    res->gpu = gpu;

    // Allocate memory for buffers, textures, and computes.
    _create_resources(res);

    dvz_obj_created(&res->obj);
}



DvzImages* dvz_resources_image(DvzResources* res, DvzTexDims dims, uvec3 shape, VkFormat format)
{
    ASSERT(res != NULL);
    ASSERT(res->gpu != NULL);
    DvzImages* img = (DvzImages*)dvz_container_alloc(&res->images);
    _make_image(res->gpu, img, dims, shape, format);
    return img;
}



DvzBuffer*
dvz_resources_buffer(DvzResources* res, DvzBufferType type, bool mappable, VkDeviceSize size)
{
    ASSERT(res != NULL);
    DvzBuffer* buffer = _make_standalone_buffer(res, type, mappable, size);
    return buffer;
}



DvzSampler* dvz_resources_sampler(DvzResources* res, VkFilter filter, VkSamplerAddressMode mode)
{
    ASSERT(res != NULL);
    DvzSampler* sampler = (DvzSampler*)dvz_container_alloc(&res->samplers);
    *sampler = dvz_sampler(res->gpu);
    dvz_sampler_min_filter(sampler, VK_FILTER_NEAREST);
    dvz_sampler_mag_filter(sampler, VK_FILTER_NEAREST);
    dvz_sampler_address_mode(sampler, DVZ_SAMPLER_AXIS_U, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    dvz_sampler_address_mode(sampler, DVZ_SAMPLER_AXIS_V, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    dvz_sampler_address_mode(sampler, DVZ_SAMPLER_AXIS_V, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    dvz_sampler_create(sampler);
    return sampler;
}



DvzCompute* dvz_resources_compute(DvzResources* res, const char* shader_path)
{
    ASSERT(res != NULL);
    ASSERT(shader_path != NULL);
    DvzCompute* compute = (DvzCompute*)dvz_container_alloc(&res->computes);
    *compute = dvz_compute(res->gpu, shader_path);
    return compute;
}



void dvz_resources_destroy(DvzResources* res)
{
    if (res == NULL)
    {
        log_error("skip destruction of null resources");
        return;
    }
    log_trace("destroying resources");
    ASSERT(res != NULL);
    ASSERT(res->gpu != NULL);

    // Destroy the resources.
    _destroy_resources(res);

    // Free the allocated memory.
    dvz_container_destroy(&res->buffers);
    dvz_container_destroy(&res->images);
    dvz_container_destroy(&res->dats);
    dvz_container_destroy(&res->texs);
    dvz_container_destroy(&res->samplers);
    dvz_container_destroy(&res->computes);

    dvz_obj_destroyed(&res->obj);
}
