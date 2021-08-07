/*************************************************************************************************/
/*  Holds all GPU data resources (buffers, images, dats, texs)                                   */
/*************************************************************************************************/

#ifndef DVZ_RESOURCES_UTILS_HEADER
#define DVZ_RESOURCES_UTILS_HEADER

#include "../include/datoviz/context.h"
#include "../include/datoviz/resources.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Default buffers                                                                              */
/*************************************************************************************************/

static DvzBuffer* _make_new_buffer(DvzResources* res)
{
    ASSERT(res != NULL);
    DvzBuffer* buffer = (DvzBuffer*)dvz_container_alloc(&res->buffers);
    *buffer = dvz_buffer(res->gpu);
    ASSERT(buffer != NULL);

    // All buffers may be accessed from these queues.
    dvz_buffer_queue_access(buffer, DVZ_DEFAULT_QUEUE_TRANSFER);
    dvz_buffer_queue_access(buffer, DVZ_DEFAULT_QUEUE_COMPUTE);
    dvz_buffer_queue_access(buffer, DVZ_DEFAULT_QUEUE_RENDER);

    return buffer;
}

static void _make_staging_buffer(DvzBuffer* buffer, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_STAGING);
    dvz_buffer_size(buffer, size);
    dvz_buffer_usage(buffer, TRANSFERABLE);
    dvz_buffer_vma_usage(buffer, VMA_MEMORY_USAGE_CPU_ONLY);
    // dvz_buffer_memory(
    //     buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));
}

static void _make_vertex_buffer(DvzBuffer* buffer, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_VERTEX);
    dvz_buffer_size(buffer, size);
    dvz_buffer_usage(
        buffer,
        TRANSFERABLE | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    // dvz_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_buffer_vma_usage(buffer, VMA_MEMORY_USAGE_GPU_ONLY);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));
}

static void _make_index_buffer(DvzBuffer* buffer, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_INDEX);
    dvz_buffer_size(buffer, size);
    dvz_buffer_usage(buffer, TRANSFERABLE | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    // dvz_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_buffer_vma_usage(buffer, VMA_MEMORY_USAGE_GPU_ONLY);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));
}

static void _make_storage_buffer(DvzBuffer* buffer, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_STORAGE);
    dvz_buffer_size(buffer, size);
    dvz_buffer_usage(buffer, TRANSFERABLE | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    // dvz_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_buffer_vma_usage(buffer, VMA_MEMORY_USAGE_GPU_ONLY);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));
}

static void _make_uniform_buffer(DvzBuffer* buffer, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_UNIFORM);
    dvz_buffer_size(buffer, size);
    dvz_buffer_usage(buffer, TRANSFERABLE | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    // dvz_buffer_memory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_buffer_vma_usage(buffer, VMA_MEMORY_USAGE_GPU_ONLY);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));
}

static void _make_mappable_buffer(DvzBuffer* buffer, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_MAPPABLE);
    dvz_buffer_size(buffer, size);
    dvz_buffer_usage(buffer, TRANSFERABLE | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    // dvz_buffer_memory(
    //     buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    dvz_buffer_vma_usage(buffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));

    // Permanently map the buffer.
    buffer->mmap = dvz_buffer_map(buffer, 0, VK_WHOLE_SIZE);
}



static void _default_buffers(DvzResources* res)
{
    ASSERT(res != NULL);
    ASSERT(res->gpu != NULL);

    // Create a predetermined set of buffers.
    for (uint32_t i = 0; i < DVZ_BUFFER_TYPE_COUNT; i++)
    {
        // IMPORTANT: buffer #i should have buffer type i
        _make_new_buffer(res);
    }

    DvzBuffer* buffer = NULL;

    // Staging buffer
    buffer = (DvzBuffer*)dvz_container_get(&res->buffers, DVZ_BUFFER_TYPE_STAGING);
    _make_staging_buffer(buffer, DVZ_BUFFER_TYPE_STAGING_SIZE);
    // Permanently map the buffer.
    // buffer->mmap = dvz_buffer_map(buffer, 0, VK_WHOLE_SIZE);

    // Vertex buffer
    buffer = (DvzBuffer*)dvz_container_get(&res->buffers, DVZ_BUFFER_TYPE_VERTEX);
    _make_vertex_buffer(buffer, DVZ_BUFFER_TYPE_VERTEX_SIZE);

    // Index buffer
    buffer = (DvzBuffer*)dvz_container_get(&res->buffers, DVZ_BUFFER_TYPE_INDEX);
    _make_index_buffer(buffer, DVZ_BUFFER_TYPE_INDEX_SIZE);

    // Storage buffer
    buffer = (DvzBuffer*)dvz_container_get(&res->buffers, DVZ_BUFFER_TYPE_STORAGE);
    _make_storage_buffer(buffer, DVZ_BUFFER_TYPE_STORAGE_SIZE);

    // Uniform buffer
    buffer = (DvzBuffer*)dvz_container_get(&res->buffers, DVZ_BUFFER_TYPE_UNIFORM);
    _make_uniform_buffer(buffer, DVZ_BUFFER_TYPE_UNIFORM_SIZE);

    // Mappable uniform buffer
    buffer = (DvzBuffer*)dvz_container_get(&res->buffers, DVZ_BUFFER_TYPE_MAPPABLE);
    _make_mappable_buffer(buffer, DVZ_BUFFER_TYPE_MAPPABLE_SIZE);
}



/*************************************************************************************************/
/*  Get shared or standalone buffer                                                              */
/*************************************************************************************************/

static DvzBuffer* _get_shared_buffer(DvzResources* res, DvzBufferType type)
{
    ASSERT(res != NULL);
    DvzBuffer* buffer = (DvzBuffer*)dvz_container_get(&res->buffers, (uint32_t)type);
    ASSERT(buffer->type == type);
    return buffer;
}

static DvzBuffer* _get_standalone_buffer(DvzResources* res, DvzBufferType type, VkDeviceSize size)
{
    ASSERT(res != NULL);
    DvzBuffer* buffer = _make_new_buffer(res);

    switch (type)
    {
    case DVZ_BUFFER_TYPE_STAGING:
        _make_staging_buffer(buffer, size);
        break;

    case DVZ_BUFFER_TYPE_VERTEX:
        _make_vertex_buffer(buffer, size);
        break;

    case DVZ_BUFFER_TYPE_INDEX:
        _make_index_buffer(buffer, size);
        break;

    case DVZ_BUFFER_TYPE_STORAGE:
        _make_storage_buffer(buffer, size);
        break;

    case DVZ_BUFFER_TYPE_UNIFORM:
        _make_uniform_buffer(buffer, size);
        break;

    case DVZ_BUFFER_TYPE_MAPPABLE:
        _make_mappable_buffer(buffer, size);
        break;

    default:
        log_error("unknown buffer type %d", type);
        break;
    }

    return buffer;
}



/*************************************************************************************************/
/*  Common resources                                                                             */
/*************************************************************************************************/

static void _create_resources(DvzResources* res)
{
    ASSERT(res != NULL);

    res->buffers =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzBuffer), DVZ_OBJECT_TYPE_BUFFER);
    res->images =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzImages), DVZ_OBJECT_TYPE_IMAGES);
    res->dats = //
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzDat), DVZ_OBJECT_TYPE_DAT);
    res->textures =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzTexture), DVZ_OBJECT_TYPE_TEXTURE);
    res->samplers =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzSampler), DVZ_OBJECT_TYPE_SAMPLER);
    res->computes =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzCompute), DVZ_OBJECT_TYPE_COMPUTE);
}



static void _destroy_resources(DvzResources* res)
{
    ASSERT(res != NULL);

    log_trace("context destroy buffers");
    CONTAINER_DESTROY_ITEMS(DvzBuffer, res->buffers, dvz_buffer_destroy)

    log_trace("context destroy sets of images");
    CONTAINER_DESTROY_ITEMS(DvzImages, res->images, dvz_images_destroy)

    // TODO
    // log_trace("context destroy dats");
    // CONTAINER_DESTROY_ITEMS(DvzDat, res->dats, dvz_dat_destroy)

    // log_trace("context destroy textures");
    // CONTAINER_DESTROY_ITEMS(DvzTexture, res->textures, dvz_tex_destroy)

    log_trace("context destroy samplers");
    CONTAINER_DESTROY_ITEMS(DvzSampler, res->samplers, dvz_sampler_destroy)

    log_trace("context destroy computes");
    CONTAINER_DESTROY_ITEMS(DvzCompute, res->computes, dvz_compute_destroy)
}



static void _default_resources(DvzResources* res)
{
    ASSERT(res != NULL);

    // TODO
    // // Create the default buffers.
    // _default_buffers(res);

    // // Create the font atlas and assign it to the context.
    // context->font_atlas = dvz_font_atlas(context);

    // // Color texture.
    // context->color_texture.arr = _load_colormaps();
    // context->color_texture.texture =
    //     dvz_ctx_texture(context, 2, (uvec3){256, 256, 1}, VK_FORMAT_R8G8B8A8_UNORM);
    // dvz_texture_address_mode(
    //     context->color_texture.texture, DVZ_TEXTURE_AXIS_U,
    //     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    // dvz_texture_address_mode(
    //     context->color_texture.texture, DVZ_TEXTURE_AXIS_V,
    //     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    // dvz_context_colormap(context);

    // // Default 1D texture, for transfer functions.
    // context->transfer_texture = _default_transfer_texture(context);
}



#ifdef __cplusplus
}
#endif

#endif
