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

static inline bool _is_buffer_mappable(DvzBuffer* buffer)
{
    ASSERT(buffer != NULL);
    return ((buffer->memory & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0);
}



static inline VkBufferUsageFlags _find_buffer_usage(DvzBufferType type)
{
    VkBufferUsageFlags usage = 0;
    switch (type)
    {
    case DVZ_BUFFER_TYPE_VERTEX:
        usage = TRANSFERABLE |                      //
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | //
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;

    case DVZ_BUFFER_TYPE_INDEX:
        usage = TRANSFERABLE | //
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        break;

    case DVZ_BUFFER_TYPE_STORAGE:
        usage = TRANSFERABLE | //
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;

    case DVZ_BUFFER_TYPE_UNIFORM:
        usage = TRANSFERABLE | //
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;

    default:
        break;
    }
    return usage;
}



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



// NOT for staging
static void
_make_shared_buffer(DvzBuffer* buffer, DvzBufferType type, bool mappable, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_size(buffer, size);
    VkBufferUsageFlags usage = _find_buffer_usage(type);
    dvz_buffer_usage(buffer, usage);
    dvz_buffer_type(buffer, type);
    dvz_buffer_vma_usage(
        buffer, mappable ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));
}



static void _make_staging_buffer(DvzBuffer* buffer, VkDeviceSize size)
{
    ASSERT(buffer != NULL);
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_STAGING);
    dvz_buffer_size(buffer, size);
    dvz_buffer_usage(buffer, TRANSFERABLE);
    dvz_buffer_vma_usage(buffer, VMA_MEMORY_USAGE_CPU_ONLY);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));
}



static DvzBuffer*
_make_standalone_buffer(DvzResources* res, DvzBufferType type, bool mappable, VkDeviceSize size)
{
    ASSERT(res != NULL);
    DvzBuffer* buffer = _make_new_buffer(res);
    if (type == DVZ_BUFFER_TYPE_STAGING)
        _make_staging_buffer(buffer, size);
    else
    {
        _make_shared_buffer(buffer, type, mappable, size);
    }
    return buffer;
}



static DvzBuffer* _find_shared_buffer(DvzResources* res, DvzBufferType type, bool mappable)
{
    ASSERT(res != NULL);
    DvzContainerIterator iter = dvz_container_iterator(&res->buffers);
    DvzBuffer* buffer = NULL;
    while (iter.item != NULL)
    {
        buffer = (DvzBuffer*)iter.item;
        ASSERT(buffer != NULL);
        if (buffer->type == type && mappable && _is_buffer_mappable(buffer))
            return buffer;
        dvz_container_iter(&iter);
    }
    return NULL;
}



// Get an existing shared buffer, or create a new one if needed.
static DvzBuffer* _get_shared_buffer(DvzResources* res, DvzBufferType type, bool mappable)
{
    ASSERT(res != NULL);
    DvzBuffer* buffer = _find_shared_buffer(res, type, mappable);
    if (buffer == NULL)
        buffer = _make_standalone_buffer(res, type, mappable, DVZ_BUFFER_DEFAULT_SIZE);
    ASSERT(buffer != NULL);
    return buffer;
}



/*************************************************************************************************/
/*  Creation of buffer regions and images                                                        */
/*************************************************************************************************/

// Only for testing: bypass the resources system, useful for testing other modules
static DvzBufferRegions
_standalone_buffer_regions(DvzGpu* gpu, DvzBufferType type, uint32_t count, VkDeviceSize size)
{
    ASSERT(gpu != NULL);
    DvzBuffer* buffer = (DvzBuffer*)calloc(1, sizeof(DvzBuffer));
    *buffer = dvz_buffer(gpu);
    if (type == DVZ_BUFFER_TYPE_STAGING)
        _make_staging_buffer(buffer, size * count);
    else if (type == DVZ_BUFFER_TYPE_VERTEX)
        _make_shared_buffer(buffer, DVZ_BUFFER_TYPE_VERTEX, true, size * count);
    DvzBufferRegions stg = dvz_buffer_regions(buffer, count, 0, size, 0);
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

static void _make_image(DvzGpu* gpu, DvzImages* img, DvzTexDims dims, uvec3 shape, VkFormat format)
{
    ASSERT(img != NULL);
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
}

static DvzImages* _standalone_image(DvzGpu* gpu, DvzTexDims dims, uvec3 shape, VkFormat format)
{
    ASSERT(gpu != NULL);
    ASSERT(1 <= dims && dims <= 3);
    log_debug(
        "creating %dD image with shape %dx%dx%d and format %d", //
        dims, shape[0], shape[1], shape[2], format);

    DvzImages* img = calloc(1, sizeof(DvzImages));
    _make_image(gpu, img, dims, shape, format);
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
