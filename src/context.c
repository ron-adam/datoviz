#include "../include/datoviz/context.h"
#include "allocs_utils.h"
#include "context_utils.h"
#include "font_atlas.h"
#include "resources_utils.h"
#include "transfer_utils.h"
#include "vklite_utils.h"
#include <stdlib.h>



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define TRANSFERABLE (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)

#define DVZ_BUFFER_TYPE_STAGING_SIZE  (4 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_VERTEX_SIZE   (4 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_INDEX_SIZE    (4 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_STORAGE_SIZE  (1 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_UNIFORM_SIZE  (1 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_MAPPABLE_SIZE DVZ_BUFFER_TYPE_UNIFORM_SIZE



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

static void _default_queues(DvzGpu* gpu, bool has_present_queue)
{
    dvz_gpu_queue(gpu, DVZ_DEFAULT_QUEUE_TRANSFER, DVZ_QUEUE_TRANSFER);
    dvz_gpu_queue(gpu, DVZ_DEFAULT_QUEUE_COMPUTE, DVZ_QUEUE_COMPUTE);
    dvz_gpu_queue(gpu, DVZ_DEFAULT_QUEUE_RENDER, DVZ_QUEUE_RENDER);
    if (has_present_queue)
        dvz_gpu_queue(gpu, DVZ_DEFAULT_QUEUE_PRESENT, DVZ_QUEUE_PRESENT);
}



static void _gpu_default_features(DvzGpu* gpu)
{
    ASSERT(gpu != NULL);
    dvz_gpu_request_features(gpu, (VkPhysicalDeviceFeatures){.independentBlend = true});
}



/*************************************************************************************************/
/*  Context                                                                                      */
/*************************************************************************************************/

void dvz_gpu_default(DvzGpu* gpu, DvzWindow* window)
{
    ASSERT(gpu != NULL);

    // Specify the default queues.
    _default_queues(gpu, window != NULL);

    // Default features
    _gpu_default_features(gpu);

    // Create the GPU after the default queues have been set.
    if (!dvz_obj_is_created(&gpu->obj))
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (window != NULL)
            surface = window->surface;
        dvz_gpu_create(gpu, surface);
    }
}



DvzContext* dvz_context(DvzGpu* gpu)
{
    ASSERT(gpu != NULL);
    ASSERT(dvz_obj_is_created(&gpu->obj));
    log_trace("creating context");

    DvzContext* ctx = calloc(1, sizeof(DvzContext));
    ASSERT(ctx != NULL);
    ctx->gpu = gpu;

    // Create the transfers.
    dvz_transfers(gpu, &ctx->transfers);

    // Create the resources.
    dvz_resources(gpu, &ctx->res);

    // Create the allocs.
    dvz_allocs(gpu, &ctx->allocs);

    // HACK: the vklite module makes the assumption that the queue #0 supports transfers.
    // Here, in the context, we make the same assumption. The first queue is reserved to transfers.
    ASSERT(DVZ_DEFAULT_QUEUE_TRANSFER == 0);

    // Create the context.
    gpu->context = ctx;
    dvz_obj_created(&ctx->obj);

    return ctx;
}



void dvz_context_reset(DvzContext* ctx)
{
    ASSERT(ctx != NULL);
    log_trace("reset the context");
    _destroy_resources(&ctx->res);
    _default_resources(&ctx->res);
}



void dvz_app_reset(DvzApp* app)
{
    ASSERT(app != NULL);
    dvz_app_wait(app);
    DvzContainerIterator iter = dvz_container_iterator(&app->gpus);
    DvzGpu* gpu = NULL;
    while (iter.item != NULL)
    {
        gpu = iter.item;
        ASSERT(gpu != NULL);
        if (dvz_obj_is_created(&gpu->obj) && gpu->context != NULL)
            dvz_context_reset(gpu->context);
        dvz_container_iter(&iter);
    }
    dvz_app_wait(app);
}



void dvz_context_destroy(DvzContext* ctx)
{
    if (ctx == NULL)
    {
        log_error("skip destruction of null context");
        return;
    }
    log_trace("destroying context");
    ASSERT(ctx != NULL);
    ASSERT(ctx->gpu != NULL);

    // Destroy the companion objects.
    dvz_transfers_destroy(&ctx->transfers);
    dvz_resources_destroy(&ctx->res);
    dvz_allocs_destroy(&ctx->allocs);
}



/*************************************************************************************************/
/*  Dats                                                                                         */
/*************************************************************************************************/

DvzDat* dvz_dat(DvzContext* ctx, DvzBufferType type, VkDeviceSize size, uint32_t count, int flags)
{
    ASSERT(ctx != NULL);
    ASSERT(size > 0);
    ASSERT(count > 0);
    ASSERT(count <= 10); // consistency check

    DvzDat* dat = (DvzDat*)dvz_container_alloc(&ctx->res.dats);
    dat->context = ctx;
    dat->flags = flags;
    bool shared = (flags & DVZ_DAT_FLAGS_SHARED) > 0;
    VkDeviceSize alignment = _find_alignment(&ctx->allocs, type);

    DvzBuffer* buffer = NULL;
    VkDeviceSize offset = 0; // to determine with allocator

    // Make sure the requested size is aligned.
    size = _align(size, alignment);

    // Shared buffer.
    if (shared)
    {
        // Get the  unique shared buffer of the requested type.
        buffer = _get_shared_buffer(&ctx->res, type);

        // Allocate a DvzDat from it.
        // NOTE: this call may resize the underlying DvzBuffer, which is slow (hard GPU sync).
        offset = _allocate_dat(&ctx->allocs, &ctx->res, type, count * size);
    }

    // Standalone buffer.
    else
    {
        // Create a brand new buffer just for this DvzDat.
        buffer = _get_standalone_buffer(&ctx->res, type, count * size);
        // Allocate the entire buffer, so offset is 0, and the size is the requested (aligned if
        // necessary) size.
        offset = 0;
    }

    // Check alignment.
    if (alignment > 0)
    {
        ASSERT(offset % alignment == 0);
        ASSERT(offset % size == 0);
    }

    // Set the buffer region.
    dat->br = dvz_buffer_regions(buffer, count, offset, size, alignment);

    dvz_obj_created(&dat->obj);
    return dat;
}



void dvz_dat_upload(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data, int flags)
{
    ASSERT(dat != NULL);
    // TODO
    // asynchronous function
    // if staging
    //     allocate staging buffer if there isn't already one
    // enqueue a buffer upload transfer
    // the copy to staging will be done in a background thread automatically
    // need the caller to call dvz_ctx_frame()
    //     dequeue all pending copies, with hard gpu sync
}



void dvz_dat_download(DvzDat* dat, VkDeviceSize size, void* data, int flags)
{
    ASSERT(dat != NULL);
    // TODO
    // asynchronous function
}



void dvz_dat_destroy(DvzDat* dat)
{
    ASSERT(dat != NULL);
    // TODO
    // free the region in the buffer
    // dvz_alloc_free();
}



/*************************************************************************************************/
/*  Texs                                                                                         */
/*************************************************************************************************/

DvzTex* dvz_tex(DvzContext* ctx, DvzTexDims dims, uvec3 shape, int flags)
{
    ASSERT(ctx != NULL);
    // TODO
    // create a new image

    return NULL;
}



void dvz_tex_upload(
    DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, int flags)
{
    ASSERT(tex != NULL);
    // TODO
}



void dvz_tex_download(
    DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, int flags)
{
    ASSERT(tex != NULL);
    // TODO
}



void dvz_tex_destroy(DvzTex* tex)
{
    ASSERT(tex != NULL); // TODO
}
