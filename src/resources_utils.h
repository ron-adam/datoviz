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
/*  Creation of buffer regions and images                                                        */
/*************************************************************************************************/

static DvzBufferRegions
_standalone_buffer_regions(DvzGpu* gpu, DvzBufferType type, VkDeviceSize size)
{
    ASSERT(gpu != NULL);
    DvzBuffer* buffer = (DvzBuffer*)calloc(1, sizeof(DvzBuffer));
    *buffer = dvz_buffer(gpu);
    if (type == DVZ_BUFFER_TYPE_STAGING)
        _make_staging_buffer(buffer, size);
    else if (type == DVZ_BUFFER_TYPE_VERTEX)
        _make_vertex_buffer(buffer, size);
    DvzBufferRegions stg = dvz_buffer_regions(buffer, 1, 0, size, 0);
    return stg;
}

static void _destroy_buffer_regions(DvzBufferRegions br)
{
    dvz_buffer_destroy(br.buffer);
    FREE(br.buffer);
}



static VkImageType _image_type_from_dims(DvzTexDims dims)
{
    switch (dims)
    {
    case DVZ_TEX_1D:
        return VK_IMAGE_TYPE_1D;
        break;
    case DVZ_TEX_2D:
        return VK_IMAGE_TYPE_2D;
        break;
    case DVZ_TEX_3D:
        return VK_IMAGE_TYPE_3D;
        break;

    default:
        break;
    }
    log_error("invalid image dimensions %d", dims);
    return VK_IMAGE_TYPE_2D;
}

static void _transition_image(DvzImages* img)
{
    ASSERT(img != NULL);

    DvzGpu* gpu = img->gpu;
    ASSERT(gpu != NULL);

    DvzCommands cmds_ = dvz_commands(gpu, 0, 1);
    DvzCommands* cmds = &cmds_;

    dvz_cmd_reset(cmds, 0);
    dvz_cmd_begin(cmds, 0);

    DvzBarrier barrier = dvz_barrier(gpu);
    dvz_barrier_stages(&barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    dvz_barrier_images(&barrier, img);
    dvz_barrier_images_layout(&barrier, VK_IMAGE_LAYOUT_UNDEFINED, img->layout);
    dvz_barrier_images_access(&barrier, 0, VK_ACCESS_TRANSFER_READ_BIT);
    dvz_cmd_barrier(cmds, 0, &barrier);

    dvz_cmd_end(cmds, 0);
    dvz_cmd_submit_sync(cmds, 0);
}

static DvzImages* _standalone_image(DvzGpu* gpu, DvzTexDims dims, uvec3 shape, VkFormat format)
{
    ASSERT(gpu != NULL);
    ASSERT(1 <= dims && dims <= 3);
    log_debug(
        "creating %dD image with shape %dx%dx%d and format %d", //
        dims, shape[0], shape[1], shape[2], format);

    DvzImages* img = calloc(1, sizeof(DvzImages));

    *img = dvz_images(gpu, _image_type_from_dims(dims), 1);

    // Create the image.
    dvz_images_format(img, format);
    dvz_images_size(img, shape[0], shape[1], shape[2]);
    dvz_images_tiling(img, VK_IMAGE_TILING_OPTIMAL);
    dvz_images_layout(img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    dvz_images_usage(
        img,                                                      //
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | //
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    dvz_images_memory(img, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_images_queue_access(img, DVZ_DEFAULT_QUEUE_TRANSFER);
    dvz_images_queue_access(img, DVZ_DEFAULT_QUEUE_COMPUTE);
    dvz_images_queue_access(img, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_images_create(img);

    // Immediately transition the image to its layout.
    _transition_image(img);

    return img;
}

static void _destroy_image(DvzImages* img)
{
    ASSERT(img);
    dvz_images_destroy(img);
    FREE(img);
}



/*************************************************************************************************/
/*  Common resources                                                                             */
/*************************************************************************************************/

static void _create_resources(DvzResources* res)
{
    ASSERT(res != NULL);

    res->buffers = //
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzBuffer), DVZ_OBJECT_TYPE_BUFFER);
    res->images = //
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzImages), DVZ_OBJECT_TYPE_IMAGES);
    res->dats = //
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzDat), DVZ_OBJECT_TYPE_DAT);
    res->texs = //
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzTex), DVZ_OBJECT_TYPE_TEX);
    res->samplers = //
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzSampler), DVZ_OBJECT_TYPE_SAMPLER);
    res->computes = //
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzCompute), DVZ_OBJECT_TYPE_COMPUTE);
}



static void _destroy_resources(DvzResources* res)
{
    ASSERT(res != NULL);

    log_trace("context destroy buffers");
    CONTAINER_DESTROY_ITEMS(DvzBuffer, res->buffers, dvz_buffer_destroy)

    log_trace("context destroy sets of images");
    CONTAINER_DESTROY_ITEMS(DvzImages, res->images, dvz_images_destroy)

    log_trace("context destroy dats");
    CONTAINER_DESTROY_ITEMS(DvzDat, res->dats, dvz_dat_destroy)

    log_trace("context destroy texs");
    CONTAINER_DESTROY_ITEMS(DvzTex, res->texs, dvz_tex_destroy)

    log_trace("context destroy samplers");
    CONTAINER_DESTROY_ITEMS(DvzSampler, res->samplers, dvz_sampler_destroy)

    log_trace("context destroy computes");
    CONTAINER_DESTROY_ITEMS(DvzCompute, res->computes, dvz_compute_destroy)
}



#ifdef __cplusplus
}
#endif

#endif
