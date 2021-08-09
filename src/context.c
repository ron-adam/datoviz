#include "../include/datoviz/context.h"
#include "../include/datoviz/atlases.h"
#include "datalloc_utils.h"
#include "resources_utils.h"
#include "transfer_utils.h"
#include "vklite_utils.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define TRANSFERABLE (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)



/*************************************************************************************************/
/*  Context utils                                                                                */
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
/*  Dat utils                                                                                    */
/*************************************************************************************************/

static inline bool _dat_is_standalone(DvzDat* dat)
{
    ASSERT(dat != NULL);
    return (dat->flags & DVZ_DAT_OPTIONS_STANDALONE) != 0;
}

static inline bool _dat_has_staging(DvzDat* dat)
{
    ASSERT(dat != NULL);
    return (dat->flags & DVZ_DAT_OPTIONS_MAPPABLE) == 0;
}

static inline bool _dat_is_dup(DvzDat* dat)
{
    ASSERT(dat != NULL);
    return (dat->flags & DVZ_DAT_OPTIONS_DUP) != 0;
}

static inline bool _dat_keep_on_resize(DvzDat* dat)
{
    ASSERT(dat != NULL);
    return (dat->flags & DVZ_DAT_OPTIONS_KEEP_ON_RESIZE) != 0;
}

static inline bool _dat_persistent_staging(DvzDat* dat)
{
    ASSERT(dat != NULL);
    return (dat->flags & DVZ_DAT_OPTIONS_PERSISTENT_STAGING) != 0;
}

static inline VkDeviceSize
_total_aligned_size(DvzBuffer* buffer, uint32_t count, VkDeviceSize size, VkDeviceSize* alignment)
{
    // Find the buffer alignment.
    *alignment = buffer->vma.alignment;
    // Make sure the requested size is aligned.
    return count * _align(size, *alignment);
}



/*************************************************************************************************/
/*  Dat allocation                                                                               */
/*************************************************************************************************/

static inline DvzDat* _alloc_staging(DvzContext* ctx, VkDeviceSize size)
{
    ASSERT(ctx != NULL);
    return dvz_dat(ctx, DVZ_BUFFER_TYPE_STAGING, size, 0);
}

static void _dat_alloc(DvzDat* dat, DvzBufferType type, uint32_t count, VkDeviceSize size)
{
    ASSERT(dat != NULL);
    DvzContext* ctx = dat->context;
    ASSERT(ctx != NULL);

    DvzBuffer* buffer = NULL;
    VkDeviceSize offset = 0; // to determine with allocator if shared buffer
    VkDeviceSize alignment = 0;
    VkDeviceSize tot_size = 0;

    bool shared = !_dat_is_standalone(dat);
    bool mappable = !_dat_has_staging(dat);

    log_debug(
        "allocate dat, buffer type %d, %s%ssize %s", //
        type, shared ? "shared, " : "", mappable ? "mappable, " : "", pretty_size(size));

    // Shared buffer.
    if (shared)
    {
        // Get the unique shared buffer of the requested type.
        buffer = _get_shared_buffer(&ctx->res, type, mappable);

        // Find the buffer alignment and total aligned size.
        tot_size = _total_aligned_size(buffer, count, size, &alignment);

        // Allocate a DvzDat from it.
        // NOTE: this call may resize the underlying DvzBuffer, which is slow (hard GPU sync).
        offset = _allocate_dat(&ctx->datalloc, &ctx->res, type, mappable, tot_size);
    }

    // Standalone buffer.
    else
    {
        // Create a brand new buffer just for this DvzDat.
        buffer = dvz_resources_buffer(&ctx->res, type, mappable, count * size);
        // Allocate the entire buffer, so offset is 0, and the size is the requested (aligned if
        // necessary) size.
        offset = 0;
        // NOTE: for standalone buffers, we should not need to worry about alignments at this point
    }

    // Check alignment.
    if (alignment > 0)
        ASSERT(offset % alignment == 0);

    // Set the buffer region.
    dat->br = dvz_buffer_regions(buffer, count, offset, size, alignment);
}

static void _dat_dealloc(DvzDat* dat)
{
    ASSERT(dat != NULL);
    DvzContext* ctx = dat->context;
    ASSERT(ctx != NULL);

    bool shared = !_dat_is_standalone(dat);
    bool mappable = !_dat_has_staging(dat);

    if (shared)
    {
        // Deallocate the buffer regions but keep the underlying buffer.
        _deallocate_dat(&ctx->datalloc, dat->br.buffer->type, mappable, dat->br.offsets[0]);
    }
    else
    {
        // Destroy the standalone buffer.
        dvz_buffer_destroy(dat->br.buffer);
    }
}



/*************************************************************************************************/
/*  Tex utils                                                                                    */
/*************************************************************************************************/

static inline bool _tex_persistent_staging(DvzTex* tex)
{
    ASSERT(tex != NULL);
    return (tex->flags & DVZ_TEX_OPTIONS_PERSISTENT_STAGING) != 0;
}

static inline void _copy_shape(uvec3 src, uvec3 dst) { memcpy(dst, src, sizeof(uvec3)); }



static void _tex_alloc(DvzTex* tex, DvzTexDims dims, uvec3 shape, VkFormat format)
{
    ASSERT(tex != NULL);
    DvzContext* ctx = tex->context;
    ASSERT(ctx != NULL);

    // Create a new image for the tex.
    tex->img = dvz_resources_image(&ctx->res, dims, shape, format);
}



static void _tex_dealloc(DvzTex* tex)
{
    ASSERT(tex != NULL);
    DvzContext* ctx = tex->context;
    ASSERT(ctx != NULL);

    // Destroy the image.
    dvz_images_destroy(tex->img);
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

    // Create the datalloc.
    dvz_datalloc(gpu, &ctx->res, &ctx->datalloc);

    // HACK: the vklite module makes the assumption that the queue #0 supports transfers.
    // Here, in the context, we make the same assumption. The first queue is reserved to transfers.
    ASSERT(DVZ_DEFAULT_QUEUE_TRANSFER == 0);

    // Create the context.
    gpu->context = ctx;
    dvz_obj_created(&ctx->obj);

    // Create the atlases.
    dvz_atlases(ctx, &ctx->atlases);

    return ctx;
}



void dvz_context_reset(DvzContext* ctx)
{
    ASSERT(ctx != NULL);
    log_trace("reset the context");
    _destroy_resources(&ctx->res);
    dvz_atlases(ctx, &ctx->atlases);
}



void dvz_context_img_count(DvzContext* ctx, uint32_t img_count)
{
    ASSERT(ctx != NULL);
    ctx->img_count = img_count;
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
    dvz_datalloc_destroy(&ctx->datalloc);
    dvz_atlases_destroy(&ctx->atlases);
}



/*************************************************************************************************/
/*  Dats                                                                                         */
/*************************************************************************************************/

DvzDat* dvz_dat(DvzContext* ctx, DvzBufferType type, VkDeviceSize size, int flags)
{
    ASSERT(ctx != NULL);
    ASSERT(size > 0);

    DvzDat* dat = (DvzDat*)dvz_container_alloc(&ctx->res.dats);
    dat->context = ctx;
    dat->flags = flags;

    // Find the number of copies.
    uint32_t count = _dat_is_dup(dat) ? ctx->img_count : 1;
    if (count == 0)
    {
        log_warn("DvzContext.img_count is not set");
        count = DVZ_MAX_SWAPCHAIN_IMAGES;
    }
    ASSERT(count > 0);
    ASSERT(count <= DVZ_MAX_SWAPCHAIN_IMAGES);
    _dat_alloc(dat, type, count, size);

    // Allocate a permanent staging dat.
    // TODO: staging standalone or not?
    if (_dat_persistent_staging(dat))
    {
        log_debug("allocate persistent staging for dat with size %s", pretty_size(size));
        dat->stg = _alloc_staging(ctx, size);
    }

    dvz_obj_created(&dat->obj);
    return dat;
}



void dvz_dat_upload(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data, bool wait)
{
    ASSERT(dat != NULL);
    DvzContext* ctx = dat->context;
    ASSERT(ctx != NULL);

    DvzTransfers* transfers = &ctx->transfers;
    ASSERT(transfers != NULL);

    // Do we need a staging buffer?
    DvzDat* stg = dat->stg;
    if (_dat_has_staging(dat) && stg == NULL)
    {
        // Need to allocate a temporary staging buffer.
        ASSERT(!_dat_persistent_staging(dat));
        stg = _alloc_staging(ctx, size);
    }

    // Enqueue the transfer task corresponding to the flags.
    bool dup = _dat_is_dup(dat);
    bool staging = stg != NULL;
    DvzBufferRegions stg_br = staging ? stg->br : (DvzBufferRegions){0};

    log_debug("upload %s to dat%s", pretty_size(size), staging ? " (with staging)" : "");

    if (!dup)
    {
        // Enqueue a standard upload task, with or without staging buffer.
        _enqueue_buffer_upload(&transfers->deq, dat->br, offset, stg_br, 0, size, data);
        if (wait)
        {
            if (staging)
                dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
            else
            {
                // WARNING: for mappable buffers, the transfer is done on the main thread (using
                // the COPY queue, not the UD queue), not in the background thread, so we need to
                // dequeue the COPY queue manually!
                dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
                dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
            }
            // dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_UD, true);
        }
    }

    else
    {
        // Enqueue a dup transfer task, with or without staging buffer.
        _enqueue_dup_transfer(&transfers->deq, dat->br, offset, stg_br, 0, size, data);
        if (wait)
        {

            // IMPORTANT: before calling the dvz_transfers_frame(), we must wait for the DUP task
            // to be in the queue. Here we dequeue it manually. The callback will add it to the
            // special Dups structure, and it will be correctly processed by dvz_transfer_frame().
            dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_DUP, true);

            ASSERT(dat->br.count > 0);
            for (uint32_t i = 0; i < dat->br.count; i++)
                dvz_transfers_frame(transfers, i);
        }
    }
}



void dvz_dat_download(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data, bool wait)
{
    ASSERT(dat != NULL);
    DvzContext* ctx = dat->context;
    ASSERT(ctx != NULL);

    DvzTransfers* transfers = &ctx->transfers;
    ASSERT(transfers != NULL);

    // Do we need a staging buffer?
    DvzDat* stg = dat->stg;
    if (_dat_has_staging(dat) && stg == NULL)
    {
        // Need to allocate a temporary staging buffer.
        ASSERT(!_dat_persistent_staging(dat));
        stg = _alloc_staging(ctx, size);
    }

    // Enqueue the transfer task corresponding to the flags.
    bool staging = stg != NULL;
    DvzBufferRegions stg_br = staging ? stg->br : (DvzBufferRegions){0};

    log_debug("download %s from dat%s", pretty_size(size), staging ? " (with staging)" : "");

    // Enqueue a standard download task, with or without staging buffer.
    _enqueue_buffer_download(&transfers->deq, dat->br, offset, stg_br, 0, size, data);

    if (wait)
    {

        if (staging)
            dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
        else
            // dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_UD, true);
            dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

        // Wait until the download finished event has been raised.
        dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);
    }
}



void dvz_dat_resize(DvzDat* dat, VkDeviceSize new_size)
{
    ASSERT(dat != NULL);
    ASSERT(dat->br.buffer != NULL);
    _dat_dealloc(dat);

    // Resize the persistent staging dat if there is one.
    if (dat->stg != NULL)
        dvz_dat_resize(dat->stg, new_size);

    _dat_alloc(dat, dat->br.buffer->type, dat->br.count, new_size);
}



void dvz_dat_destroy(DvzDat* dat)
{
    ASSERT(dat != NULL);
    DvzContext* ctx = dat->context;
    ASSERT(ctx != NULL);
    _dat_dealloc(dat);

    // Destroy the persistent staging dat if there is one.
    if (dat->stg != NULL)
        dvz_dat_destroy(dat->stg);

    dvz_obj_destroyed(&dat->obj);
}



/*************************************************************************************************/
/*  Texs                                                                                         */
/*************************************************************************************************/

static DvzDat* _tex_staging(DvzTex* tex, VkDeviceSize size)
{
    ASSERT(tex != NULL);
    ASSERT(tex->context != NULL);
    DvzDat* stg = tex->stg;
    if (stg != NULL)
        return stg;

    // Need to allocate a staging buffer.
    log_debug("allocate persistent staging buffer with size %s for tex", pretty_size(size));
    stg = _alloc_staging(tex->context, size);

    // If persistent staging, store it.
    if (_tex_persistent_staging(tex))
        tex->stg = stg;

    return stg;
}



DvzTex* dvz_tex(DvzContext* ctx, DvzTexDims dims, uvec3 shape, VkFormat format, int flags)
{
    ASSERT(ctx != NULL);

    DvzTex* tex = (DvzTex*)dvz_container_alloc(&ctx->res.texs);
    tex->context = ctx;
    tex->flags = flags;
    tex->dims = dims;
    _copy_shape(shape, tex->shape);

    // Allocate the tex.
    // TODO: GPU sync before?
    _tex_alloc(tex, dims, shape, format);

    dvz_obj_created(&tex->obj);
    return tex;
}



void dvz_tex_upload(
    DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, bool wait)
{
    ASSERT(tex != NULL);
    ASSERT(tex->img != NULL);

    DvzContext* ctx = tex->context;
    ASSERT(ctx != NULL);

    DvzTransfers* transfers = &tex->context->transfers;

    // Get the associated staging buffer.
    DvzDat* stg = _tex_staging(tex, size);
    ASSERT(stg != NULL);

    _enqueue_image_upload(&transfers->deq, tex->img, offset, shape, stg->br, 0, size, data);

    if (wait)
    {
        dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
    }
}



void dvz_tex_download(
    DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, bool wait)
{
    ASSERT(tex != NULL);
    ASSERT(tex->img != NULL);

    DvzContext* ctx = tex->context;
    ASSERT(ctx != NULL);

    DvzTransfers* transfers = &tex->context->transfers;

    // Get the associated staging buffer.
    DvzDat* stg = _tex_staging(tex, size);
    ASSERT(stg != NULL);

    _enqueue_image_download(&transfers->deq, tex->img, offset, shape, stg->br, 0, size, data);

    if (wait)
    {
        dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_CPY, true);
        dvz_deq_dequeue(&transfers->deq, DVZ_TRANSFER_PROC_EV, true);
    }
}



void dvz_tex_resize(DvzTex* tex, uvec3 new_shape, VkDeviceSize new_size)
{
    ASSERT(tex != NULL);
    ASSERT(tex->img != NULL);

    // TODO: GPU sync before?
    dvz_images_resize(tex->img, new_shape[0], new_shape[1], new_shape[2]);

    // Resize the persistent staging tex if there is one.
    if (tex->stg != NULL)
        dvz_dat_resize(tex->stg, new_size);
}



void dvz_tex_destroy(DvzTex* tex)
{
    ASSERT(tex != NULL);

    // Deallocate the tex.
    _tex_dealloc(tex);

    // Destroy the persistent staging tex if there is one.
    if (tex->stg != NULL)
        dvz_dat_destroy(tex->stg);

    dvz_obj_destroyed(&tex->obj);
}
