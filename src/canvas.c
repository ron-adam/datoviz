#include "../include/datoviz/canvas.h"
#include "../include/datoviz/app.h"
#include "../include/datoviz/context.h"
#include "../include/datoviz/input.h"
#include "../include/datoviz/vklite.h"
#include "canvas_utils.h"
#include "events_utils.h"
#include "transfer_utils.h"
#include "vklite_utils.h"

#include <stdlib.h>



// NOTE: all functions here are to be used from the main thread. Other user-exposed canvas
// functions will be defined in run.c as they will require task enqueue in the main canvas queue.



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Utils                                                                                    */
/*************************************************************************************************/

static DvzImages
_staging_image(DvzCanvas* canvas, VkFormat format, uint32_t width, uint32_t height)
{
    ASSERT(canvas != NULL);
    ASSERT(width > 0);
    ASSERT(height > 0);

    DvzImages staging = dvz_images(canvas->gpu, VK_IMAGE_TYPE_2D, 1);
    dvz_images_format(&staging, format);
    dvz_images_size(&staging, width, height, 1);
    dvz_images_tiling(&staging, VK_IMAGE_TILING_LINEAR);
    dvz_images_usage(&staging, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    dvz_images_layout(&staging, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    dvz_images_memory(
        &staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    dvz_images_create(&staging);
    dvz_images_transition(&staging);
    return staging;
}



DvzViewport dvz_viewport_default(uint32_t width, uint32_t height)
{
    DvzViewport viewport = {0};

    viewport.viewport.x = 0;
    viewport.viewport.y = 0;
    viewport.viewport.minDepth = +0;
    viewport.viewport.maxDepth = +1;

    viewport.size_framebuffer[0] = viewport.viewport.width = (float)width;
    viewport.size_framebuffer[1] = viewport.viewport.height = (float)height;
    viewport.size_screen[0] = viewport.size_framebuffer[0];
    viewport.size_screen[1] = viewport.size_framebuffer[1];

    return viewport;
}



DvzViewport dvz_viewport_full(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    DvzViewport viewport = {0};

    viewport.viewport.x = 0;
    viewport.viewport.y = 0;
    viewport.viewport.minDepth = +0;
    viewport.viewport.maxDepth = +1;

    ASSERT(canvas->render.swapchain.images != NULL);
    viewport.size_framebuffer[0] = viewport.viewport.width =
        (float)canvas->render.swapchain.images->width;
    viewport.size_framebuffer[1] = viewport.viewport.height =
        (float)canvas->render.swapchain.images->height;

    if (canvas->window != NULL)
    {
        viewport.size_screen[0] = canvas->window->width;
        viewport.size_screen[1] = canvas->window->height;
    }
    else
    {
        // If offscreen canvas, there is no window and we use the same units for screen coords
        // and framebuffer coords.
        viewport.size_screen[0] = viewport.size_framebuffer[0];
        viewport.size_screen[1] = viewport.size_framebuffer[1];
    }

    viewport.clip = DVZ_VIEWPORT_FULL;

    return viewport;
}



/*************************************************************************************************/
/*  Canvas initialization                                                                        */
/*************************************************************************************************/

static DvzCanvas*
_canvas(DvzGpu* gpu, uint32_t width, uint32_t height, bool offscreen, bool overlay, int flags)
{
    ASSERT(gpu != NULL);
    DvzApp* app = gpu->app;

    ASSERT(app != NULL);
    // HACK: create the canvas container here because vklite.c does not know the size of DvzCanvas.
    if (app->canvases.capacity == 0)
    {
        log_trace("create canvases container");
        app->canvases =
            dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzCanvas), DVZ_OBJECT_TYPE_CANVAS);
    }

    log_debug("initialize new canvas");

    DvzCanvas* canvas = dvz_container_alloc(&app->canvases);
    canvas->app = app;
    canvas->gpu = gpu;
    canvas->offscreen = offscreen;
    canvas->init_size[0] = width;
    canvas->init_size[1] = height;
    canvas->running = true;
    canvas->vsync = true; // by default, vsync is enabled (caps at 60 FPS)

    canvas->dpi_scaling = DVZ_DEFAULT_DPI_SCALING;
    // int flag_dpi = flags >> 12;
    // if (flag_dpi > 0)
    //     canvas->dpi_scaling *= (.5 * flag_dpi);

    // canvas->overlay = overlay;
    // canvas->flags = flags;

    // bool show_fps = _show_fps(canvas);
    // bool support_pick = _support_pick(canvas);
    // log_trace("creating canvas with show_fps=%d, support_pick=%d", show_fps, support_pick);

    // Allocate memory for canvas objects.
    canvas->commands =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzCommands), DVZ_OBJECT_TYPE_COMMANDS);
    canvas->graphics =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzGraphics), DVZ_OBJECT_TYPE_GRAPHICS);

    dvz_obj_init(&canvas->obj);

    return canvas;
}



DvzCanvas* dvz_canvas(DvzGpu* gpu, uint32_t width, uint32_t height, int flags)
{
    ASSERT(gpu != NULL);
    bool offscreen = ((flags & DVZ_CANVAS_FLAGS_OFFSCREEN) != 0) ||
                     (gpu->app->backend == DVZ_BACKEND_GLFW ? false : true);
    bool overlay = (flags & DVZ_CANVAS_FLAGS_IMGUI) != 0;
    if (offscreen && overlay)
    {
        log_warn("overlay is not supported in offscreen mode, disabling it");
        overlay = false;
    }

#if SWIFTSHADER
    log_warn("swiftshader mode is active, forcing offscreen rendering");
    offscreen = true;
    overlay = false;
#endif

    return _canvas(gpu, width, height, offscreen, overlay, flags);
}



void dvz_canvas_reset(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);
    dvz_app_wait(canvas->app);

    ASSERT(canvas->gpu != NULL);
    ASSERT(canvas->gpu->context != NULL);
    dvz_context_reset(canvas->gpu->context);

    _clock_init(&canvas->clock);
    canvas->cur_frame = 0;
    canvas->frame_idx = 0;
    canvas->fps.last_frame_idx = 0;
}



/*************************************************************************************************/
/*  Canvas creation                                                                              */
/*************************************************************************************************/

void dvz_canvas_offscreen(DvzCanvas* canvas, bool is_offscreen)
{
    ASSERT(canvas != NULL);
    canvas->offscreen = is_offscreen;
}



void dvz_canvas_with_pick(DvzCanvas* canvas, bool with_pick)
{
    ASSERT(canvas != NULL);
    canvas->with_pick = with_pick;
}



void dvz_canvas_with_gui(DvzCanvas* canvas, bool with_gui)
{
    ASSERT(canvas != NULL);
    canvas->overlay = with_gui;
}



void dvz_canvas_vsync(DvzCanvas* canvas, bool vsync)
{
    ASSERT(canvas != NULL);
    canvas->vsync = vsync;
}



static void _canvas_fps(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);

    _clock_init(&canvas->clock);

    canvas->fps.fps = 100;
    canvas->fps.efps = 100;

    // TODO:
    // Compute FPS every 100 ms, even if FPS is not shown (so that the value remains accessible
    // in callbacks if needed).
    // dvz_event_callback(canvas, DVZ_EVENT_TIMER, .1, DVZ_EVENT_MODE_SYNC, _fps_callback, NULL);

    // if (show_fps)
    //     dvz_event_callback(
    //         canvas, DVZ_EVENT_IMGUI, 0, DVZ_EVENT_MODE_SYNC, dvz_gui_callback_fps, NULL);
}

static void _canvas_window(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);

    if (canvas->offscreen)
        return;

    log_trace("creating canvas window");
    DvzWindow* window = dvz_window(canvas->app, canvas->init_size[0], canvas->init_size[1]);

    if (window == NULL)
    {
        log_error("window creation failed, forcing offscreen mode for the canvas");
        canvas->offscreen = true;
    }
    else
    {
        ASSERT(window->app == canvas->app);
        ASSERT(window->app != NULL);
        canvas->window = window;

        uint32_t framebuffer_width, framebuffer_height;
        // NOTE: function name unclear, this call sets the window size...
        dvz_window_get_size(window, &framebuffer_width, &framebuffer_height);
        ASSERT(framebuffer_width > 0);
        ASSERT(framebuffer_height > 0);
    }
}

static void _canvas_ensure_context(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    DvzGpu* gpu = canvas->gpu;

    // Automatic creation of GPU with default queues and features.
    if (!dvz_obj_is_created(&gpu->obj))
        dvz_gpu_default(gpu, canvas->window);

    // Automatic creation of GPU context.
    if (gpu->context == NULL || !dvz_obj_is_created(&gpu->context->obj))
    {
        log_trace("canvas automatically create the GPU context");
        gpu->context = dvz_context(gpu);
    }
}

static void _canvas_renderpass(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    log_trace("creating canvas renderpass");
    canvas->render.renderpass = default_renderpass(
        canvas->gpu, DVZ_DEFAULT_BACKGROUND, DVZ_DEFAULT_IMAGE_FORMAT, canvas->overlay,
        canvas->with_pick);
    if (canvas->overlay)
        canvas->render.renderpass_overlay = default_renderpass_overlay(
            canvas->gpu, DVZ_DEFAULT_IMAGE_FORMAT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

static void _offscreen_images(DvzCanvas* canvas, DvzImages* images)
{
    ASSERT(images != NULL);

    // Color attachment
    dvz_images_format(images, canvas->render.renderpass.attachments[0].format);
    dvz_images_size(images, canvas->init_size[0], canvas->init_size[1], 1);
    dvz_images_tiling(images, VK_IMAGE_TILING_OPTIMAL);
    dvz_images_usage(
        images, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    dvz_images_memory(images, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_images_aspect(images, VK_IMAGE_ASPECT_COLOR_BIT);
    dvz_images_layout(images, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    dvz_images_queue_access(images, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_images_create(images);
}

static void _screencast_staging(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->gpu != NULL);

    DvzContext* ctx = canvas->gpu->context;
    ASSERT(ctx != NULL);

    canvas->render.screencast_staging = dvz_buffer(ctx->gpu);
    DvzBuffer* buffer = &canvas->render.screencast_staging;
    dvz_buffer_queue_access(buffer, DVZ_DEFAULT_QUEUE_TRANSFER);
    VkBufferUsageFlagBits transferable =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    dvz_buffer_type(buffer, DVZ_BUFFER_TYPE_STAGING);
    dvz_buffer_size(buffer, 1600 * 1200 * 4); // default size
    dvz_buffer_usage(buffer, transferable);
    dvz_buffer_memory(
        buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    dvz_buffer_create(buffer);
    ASSERT(dvz_obj_is_created(&buffer->obj));

    // Also create a texture wrapper around the swapchain image so as to user the transfer API.
    DvzTexture* tex = &canvas->render.screencast_tex;
    tex->context = ctx;
    tex->dims = 2;
    tex->image = canvas->render.swapchain.images;
    dvz_obj_created(&tex->obj);
}

static void _canvas_swapchain(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);

    log_trace("creating canvas swapchain");
    dvz_swapchain_format(&canvas->render.swapchain, DVZ_DEFAULT_IMAGE_FORMAT);

    if (!canvas->offscreen)
    {
        dvz_swapchain_present_mode(
            &canvas->render.swapchain,
            canvas->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR);
        dvz_swapchain_create(&canvas->render.swapchain);

        ASSERT(canvas->render.swapchain.images != NULL);
    }
    else
    {
        canvas->render.swapchain.images = calloc(1, sizeof(DvzImages));
        ASSERT(canvas->render.swapchain.img_count == 1);
        *canvas->render.swapchain.images =
            dvz_images(canvas->render.swapchain.gpu, VK_IMAGE_TYPE_2D, 1);
        // Create the offscreen image.
        _offscreen_images(canvas, canvas->render.swapchain.images);
        dvz_obj_created(&canvas->render.swapchain.obj);
    }

    // Create a staging buffer, to be used by screencast and screenshot.
    // It is automatically recreated upon resize.
    _screencast_staging(canvas);
}

static void _canvas_attachments(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    DvzGpu* gpu = canvas->gpu;

    log_trace("creating canvas attachments");

    // Depth attachment.
    canvas->render.depth_image = dvz_images(gpu, VK_IMAGE_TYPE_2D, 1);
    depth_image(
        &canvas->render.depth_image, &canvas->render.renderpass, //
        canvas->render.swapchain.images->width, canvas->render.swapchain.images->height);

    // Pick attachment.
    if (canvas->with_pick)
    {
        canvas->render.pick_image = dvz_images(gpu, VK_IMAGE_TYPE_2D, 1);
        pick_image(
            &canvas->render.pick_image, &canvas->render.renderpass, //
            canvas->render.swapchain.images->width, canvas->render.swapchain.images->height);
        canvas->render.pick_staging = _staging_image(
            canvas, canvas->render.pick_image.format, DVZ_PICK_STAGING_SIZE,
            DVZ_PICK_STAGING_SIZE);
    }
}

static void _canvas_framebuffers(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    DvzGpu* gpu = canvas->gpu;

    log_trace("creating canvas framebuffers");

    canvas->render.framebuffers = dvz_framebuffers(gpu);
    dvz_framebuffers_attachment(&canvas->render.framebuffers, 0, canvas->render.swapchain.images);
    dvz_framebuffers_attachment(&canvas->render.framebuffers, 1, &canvas->render.depth_image);
    if (canvas->with_pick)
        dvz_framebuffers_attachment(&canvas->render.framebuffers, 2, &canvas->render.pick_image);
    dvz_framebuffers_create(&canvas->render.framebuffers, &canvas->render.renderpass);

    if (canvas->overlay)
    {
        canvas->render.framebuffers_overlay = dvz_framebuffers(gpu);
        dvz_framebuffers_attachment(
            &canvas->render.framebuffers_overlay, 0, canvas->render.swapchain.images);
        dvz_framebuffers_create(
            &canvas->render.framebuffers_overlay, &canvas->render.renderpass_overlay);
    }
}

static void _canvas_sync(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    DvzGpu* gpu = canvas->gpu;

    log_trace("creating canvas sync objects");

    uint32_t frames_in_flight = canvas->offscreen ? 1 : DVZ_MAX_FRAMES_IN_FLIGHT;

    canvas->sync.sem_img_available = dvz_semaphores(gpu, frames_in_flight);
    canvas->sync.sem_render_finished = dvz_semaphores(gpu, frames_in_flight);
    canvas->sync.present_semaphores = &canvas->sync.sem_render_finished;

    canvas->sync.fences_render_finished = dvz_fences(gpu, frames_in_flight, true);
    canvas->sync.fences_flight.gpu = gpu;
    canvas->sync.fences_flight.count = canvas->render.swapchain.img_count;
}

static void _canvas_commands(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);

    log_trace("creating canvas command buffers");

    // Default transfer commands.
    canvas->cmds_transfer = dvz_commands(canvas->gpu, DVZ_DEFAULT_QUEUE_TRANSFER, 1);

    // Default render commands.
    canvas->cmds_render =
        dvz_commands(canvas->gpu, DVZ_DEFAULT_QUEUE_RENDER, canvas->render.swapchain.img_count);

    // Default submit.
    canvas->render.submit = dvz_submit(canvas->gpu);
}

static void _canvas_input(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);

    log_trace("creating canvas input");

    canvas->input = dvz_input();
    void* backend_window = NULL;
    if (canvas->window != NULL)
        backend_window = canvas->window->backend_window;
    dvz_input_backend(&canvas->input, canvas->app->backend, backend_window);
}



void dvz_canvas_create(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    log_debug("creating canvas");

    // Initialize the canvas local clock.
    _canvas_fps(canvas);

    // Create the window.
    _canvas_window(canvas);

    // Make sure the GPU and context have been created.
    _canvas_ensure_context(canvas);

    // Initialize the renderpass(es).
    _canvas_renderpass(canvas);

    // Create the swapchain.
    uint32_t min_img_count = canvas->offscreen ? 1 : DVZ_MIN_SWAPCHAIN_IMAGE_COUNT;
    canvas->render.swapchain = dvz_swapchain(canvas->gpu, canvas->window, min_img_count);
    _canvas_swapchain(canvas);

    // Create the attachments.
    _canvas_attachments(canvas);

    // Create the renderpass(es).
    dvz_renderpass_create(&canvas->render.renderpass);
    if (canvas->overlay)
        dvz_renderpass_create(&canvas->render.renderpass_overlay);

    // Create the framebuffers.
    _canvas_framebuffers(canvas);

    // Create synchronization objects.
    _canvas_sync(canvas);

    // Create the command buffers.
    _canvas_commands(canvas);

    // Create the Input instance.
    _canvas_input(canvas);

    // Update the viewport field.
    canvas->viewport = dvz_viewport_full(canvas);

    // The canvas is created!
    dvz_obj_created(&canvas->obj);

    log_debug(
        "successfully created canvas of size %dx%d", //
        canvas->render.swapchain.images->width,      //
        canvas->render.swapchain.images->height);
}



void dvz_canvas_recreate(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    DvzBackend backend = canvas->app->backend;
    DvzWindow* window = canvas->window;
    DvzGpu* gpu = canvas->gpu;
    DvzSwapchain* swapchain = &canvas->render.swapchain;
    DvzFramebuffers* framebuffers = &canvas->render.framebuffers;
    DvzRenderpass* renderpass = &canvas->render.renderpass;
    DvzFramebuffers* framebuffers_overlay = &canvas->render.framebuffers_overlay;
    DvzRenderpass* renderpass_overlay = &canvas->render.renderpass_overlay;
    // bool support_pick = _support_pick(canvas);

    ASSERT(window != NULL);
    ASSERT(gpu != NULL);
    ASSERT(swapchain != NULL);
    ASSERT(framebuffers != NULL);
    ASSERT(framebuffers_overlay != NULL);
    ASSERT(renderpass != NULL);
    ASSERT(renderpass_overlay != NULL);

    log_trace("recreate canvas after resize");

    // Wait until the device is ready and the window fully resized.
    // Framebuffer new size.
    uint32_t width, height;
    backend_window_get_size(
        backend, window->backend_window, //
        &window->width, &window->height, //
        &width, &height);
    dvz_gpu_wait(gpu);

    // Destroy swapchain resources.
    dvz_framebuffers_destroy(&canvas->render.framebuffers);
    if (canvas->overlay)
        dvz_framebuffers_destroy(&canvas->render.framebuffers_overlay);
    dvz_images_destroy(&canvas->render.depth_image);
    if (canvas->with_pick)
        dvz_images_destroy(&canvas->render.pick_image);
    dvz_images_destroy(canvas->render.swapchain.images);

    // Recreate the swapchain. This will automatically set the swapchain->images new size.
    dvz_swapchain_recreate(swapchain);

    // Find the new framebuffer size as determined by the swapchain recreation.
    width = swapchain->images->width;
    height = swapchain->images->height;

    // Check that we use the same DvzImages struct here.
    ASSERT(swapchain->images == framebuffers->attachments[0]);

    // Need to recreate the depth image with the new size.
    dvz_images_size(&canvas->render.depth_image, width, height, 1);
    dvz_images_create(&canvas->render.depth_image);

    // Need to recreate the staging image with the new size.
    dvz_buffer_resize(&canvas->render.screencast_staging, width * height * 4);

    if (canvas->with_pick)
    {
        // Need to recreate the pick image with the new size.
        dvz_images_size(&canvas->render.pick_image, width, height, 1);
        dvz_images_create(&canvas->render.pick_image);
    }

    // Recreate the framebuffers with the new size.
    for (uint32_t i = 0; i < framebuffers->attachment_count; i++)
    {
        ASSERT(framebuffers->attachments[i]->width == width);
        ASSERT(framebuffers->attachments[i]->height == height);
    }
    dvz_framebuffers_create(framebuffers, renderpass);
    if (canvas->overlay)
        dvz_framebuffers_create(framebuffers_overlay, renderpass_overlay);

    // Recreate the semaphores.
    dvz_app_wait(canvas->app);
    dvz_semaphores_recreate(&canvas->sync.sem_img_available);
    dvz_semaphores_recreate(&canvas->sync.sem_render_finished);
    canvas->sync.present_semaphores = &canvas->sync.sem_render_finished;
}



/*************************************************************************************************/
/*  Canvas misc                                                                                  */
/*************************************************************************************************/

void dvz_canvas_size(DvzCanvas* canvas, DvzCanvasSizeType type, uvec2 size)
{
    ASSERT(canvas != NULL);

    if (canvas->window == NULL && type == DVZ_CANVAS_SIZE_SCREEN)
    {
        ASSERT(canvas->offscreen);
        log_trace("cannot determine window size in screen coordinates with offscreen canvas");
        type = DVZ_CANVAS_SIZE_FRAMEBUFFER;
    }

    switch (type)
    {
    case DVZ_CANVAS_SIZE_SCREEN:
        ASSERT(canvas->window != NULL);
        size[0] = canvas->window->width;
        size[1] = canvas->window->height;
        break;
    case DVZ_CANVAS_SIZE_FRAMEBUFFER:
        size[0] = canvas->render.framebuffers.attachments[0]->width;
        size[1] = canvas->render.framebuffers.attachments[0]->height;
        break;
    default:
        log_warn("unknown size type %d", type);
        break;
    }
}



double dvz_canvas_aspect(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->render.swapchain.images->width > 0);
    ASSERT(canvas->render.swapchain.images->height > 0);
    return canvas->render.swapchain.images->width /
           (double)canvas->render.swapchain.images->height;
}



DvzCommands* dvz_canvas_commands(DvzCanvas* canvas, uint32_t queue_idx, uint32_t count)
{
    ASSERT(canvas != NULL);
    DvzCommands* commands = dvz_container_alloc(&canvas->commands);
    *commands = dvz_commands(canvas->gpu, queue_idx, count);
    return commands;
}



void dvz_canvas_buffers(
    DvzCanvas* canvas, DvzBufferRegions br, //
    VkDeviceSize offset, VkDeviceSize size, const void* data)
{
    ASSERT(canvas != NULL);
    ASSERT(size > 0);
    ASSERT(data != NULL);
    ASSERT(br.buffer != NULL);
    ASSERT(br.count == canvas->render.swapchain.img_count);
    if (br.buffer->type != DVZ_BUFFER_TYPE_MAPPABLE)
    {
        log_error("dvz_canvas_buffers() can only be used on mappable buffers.");
        return;
    }
    ASSERT(br.buffer->mmap != NULL);
    uint32_t idx = canvas->render.swapchain.img_idx;
    ASSERT(idx < br.count);
    dvz_buffer_upload(br.buffer, br.offsets[idx] + offset, size, data);
}



/*************************************************************************************************/
/*  Screenshot                                                                                   */
/*************************************************************************************************/

static uint8_t*
_rearrange_image(uint32_t w, uint32_t h, bool remove_alpha, bool swizzle, uint8_t* rgba)
{
    ASSERT(rgba != NULL);
    ASSERT(w > 0);
    ASSERT(h > 0);

    uint8_t* rgb = calloc(w * h, 3);
    uint32_t k = remove_alpha ? 4 : 3;
    if (swizzle)
    {
        for (uint32_t i = 0; i < w * h; i++)
        {
            rgb[3 * i + 0] = rgba[k * i + 2];
            rgb[3 * i + 1] = rgba[k * i + 1];
            rgb[3 * i + 2] = rgba[k * i + 0];
        }
    }
    else
    {
        if (remove_alpha)
        {
            for (uint32_t i = 0; i < w * h; i++)
            {
                memcpy(&rgb[3 * i], &rgba[4 * i], 3);
            }
        }
        else
        {
            memcpy(rgb, rgba, w * h * 3);
        }
    }

    FREE(rgba);
    return rgb;
}

uint8_t* dvz_screenshot(DvzCanvas* canvas, bool remove_alpha)
{
    ASSERT(canvas != NULL);

    DvzGpu* gpu = canvas->gpu;
    ASSERT(gpu != NULL);
    DvzContext* ctx = gpu->context;
    ASSERT(ctx != NULL);

    // Hard GPU synchronization.
    dvz_gpu_wait(gpu);

    DvzImages* images = canvas->render.swapchain.images;
    if (images == NULL)
    {
        log_error("empty swapchain images, aborting screenshot creation");
        return NULL;
    }

    // Staging images.
    // HACK: DvzTexture wrapper so that we can use the transfers API.
    DvzTexture* tex = &canvas->render.screencast_tex;

    // NOTE: if has_alpha = false, we can only remove it at the end.
    uint32_t ncomp = 4;

    VkDeviceSize size = images->width * images->height * ncomp;
    uvec3 shape = {images->width, images->height, images->depth};

    DvzBuffer* buf = &canvas->render.screencast_staging;
    DvzBufferRegions stg = dvz_buffer_regions(buf, 1, 0, size, 0);

    uint8_t* data = calloc(size, 1);

    // TODO
    // _enqueue_image_download(&ctx->deq, tex, (uvec3){0}, shape, stg, 0, size, data);

    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_CPY, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_UD);

    dvz_deq_dequeue(&ctx->deq, DVZ_TRANSFER_PROC_EV, true);
    dvz_deq_wait(&ctx->deq, DVZ_TRANSFER_PROC_EV);

    bool swizzle = true; // canvas->offscreen;
    data = _rearrange_image(shape[0], shape[1], remove_alpha, swizzle, data);

    // NOTE: the caller MUST free the returned pointer.
    return data;
}



void dvz_screenshot_file(DvzCanvas* canvas, const char* png_path)
{
    ASSERT(canvas != NULL);
    ASSERT(png_path != NULL);
    ASSERT(strlen(png_path) > 0);

    log_info("saving screenshot of canvas to %s with full synchronization (slow)", png_path);
    uint8_t* rgb = dvz_screenshot(canvas, false);
    if (rgb == NULL)
    {
        log_error("screenshot failed");
        return;
    }
    DvzImages* images = canvas->render.swapchain.images;
    dvz_write_png(png_path, images->width, images->height, rgb);
    FREE(rgb);
}



/*************************************************************************************************/
/*  Canvas destruction                                                                           */
/*************************************************************************************************/

void dvz_canvas_destroy(DvzCanvas* canvas)
{
    if (canvas == NULL || canvas->obj.status != DVZ_OBJECT_STATUS_CREATED)
    {
        log_trace(
            "skip destruction of already-destroyed canvas with status %d", canvas->obj.status);
        return;
    }
    log_debug("destroying canvas with status %d", canvas->obj.status);

    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);
    ASSERT(canvas->gpu != NULL);

    // Wait until all pending events have been processed.
    backend_poll_events(canvas->app->backend, canvas->window);

    // Wait on the GPU.
    dvz_gpu_wait(canvas->gpu);

    // Destroy the graphics.
    log_trace("canvas destroy graphics pipelines");
    CONTAINER_DESTROY_ITEMS(DvzGraphics, canvas->graphics, dvz_graphics_destroy)
    dvz_container_destroy(&canvas->graphics);

    // Destroy the depth and pick images.
    dvz_images_destroy(&canvas->render.depth_image);
    dvz_images_destroy(&canvas->render.pick_image);
    dvz_images_destroy(&canvas->render.pick_staging);

    // Destroy the screencast staging buffer.
    dvz_buffer_destroy(&canvas->render.screencast_staging);

    // Destroy the renderpasses.
    log_trace("canvas destroy renderpass");
    dvz_renderpass_destroy(&canvas->render.renderpass);
    if (canvas->overlay)
        dvz_renderpass_destroy(&canvas->render.renderpass_overlay);

    // Destroy the swapchain.
    log_trace("canvas destroy swapchain");
    dvz_swapchain_destroy(&canvas->render.swapchain);

    // Destroy the framebuffers.
    log_trace("canvas destroy framebuffers");
    dvz_framebuffers_destroy(&canvas->render.framebuffers);
    if (canvas->overlay)
        dvz_framebuffers_destroy(&canvas->render.framebuffers_overlay);

    // Destroy the Dear ImGui context if it was initialized.

    // HACK: we should NOT destroy imgui when using multiple DvzApp, since Dear ImGui uses
    // global context shared by all DvzApps. In practice, this is for now equivalent to using the
    // offscreen backend (which does not support Dear ImGui at the moment anyway).
    // if (canvas->app->backend != DVZ_BACKEND_OFFSCREEN)
    //     dvz_imgui_destroy();

    // Destroy the window.
    log_trace("canvas destroy window");
    if (canvas->window != NULL)
    {
        ASSERT(canvas->window->app != NULL);
        dvz_window_destroy(canvas->window);
    }

    log_trace("canvas destroy commands");
    CONTAINER_DESTROY_ITEMS(DvzCommands, canvas->commands, dvz_commands_destroy)
    dvz_container_destroy(&canvas->commands);

    // Destroy the semaphores.
    log_trace("canvas destroy semaphores");
    dvz_semaphores_destroy(&canvas->sync.sem_img_available);
    dvz_semaphores_destroy(&canvas->sync.sem_render_finished);

    // Destroy the fences.
    log_trace("canvas destroy fences");
    dvz_fences_destroy(&canvas->sync.fences_render_finished);

    FREE(canvas->render.swapchain.images);

    // Free the GUI context if it has been set.
    // FREE(canvas->gui_context);

    // CONTAINER_DESTROY_ITEMS(DvzGui, canvas->guis, dvz_gui_destroy)
    // dvz_container_destroy(&canvas->guis);

    // NOTE: the Input destruction must occur AFTER the window destruction, otherwise the window
    // glfw callbacks might enqueue input events to a destroyed deq, causing a segfault.
    dvz_input_destroy(&canvas->input);

    dvz_obj_destroyed(&canvas->obj);
}



void dvz_canvases_destroy(DvzContainer* canvases)
{
    if (canvases == NULL || canvases->capacity == 0)
        return;
    log_trace("destroy all canvases");
    CONTAINER_DESTROY_ITEMS(DvzCanvas, (*canvases), dvz_canvas_destroy)
    dvz_container_destroy(canvases);
}
