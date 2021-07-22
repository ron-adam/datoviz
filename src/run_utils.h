#ifndef DVZ_RUN_UTILS_HEADER
#define DVZ_RUN_UTILS_HEADER

#include "../include/datoviz/run.h"
#include "../include/datoviz/vklite.h"
#include "vklite_utils.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Task enqueueing                                                                              */
/*************************************************************************************************/

static void
_enqueue_canvas_event(DvzRun* run, DvzCanvas* canvas, uint32_t deq_idx, DvzCanvasEventType type)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);

    // Will be FREE-ed by the dequeue batch function in the main loop.
    DvzCanvasEvent* ev = (DvzCanvasEvent*)calloc(1, sizeof(DvzCanvasEvent));
    ev->canvas = canvas;
    dvz_deq_enqueue(&run->deq, deq_idx, (int)type, ev);
}



static void _enqueue_canvas_frame(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    // log_info("enqueue frame");

    DvzCanvasEventFrame* ev = calloc(1, sizeof(DvzCanvasEventFrame));
    ev->canvas = canvas;
    ev->frame_idx = ev->canvas->frame_idx;
    dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_FRAME, (int)DVZ_RUN_CANVAS_FRAME, ev);
}



static void _enqueue_to_refill(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    // log_info("enqueue refill");

    DvzCanvasEvent* ev = calloc(1, sizeof(DvzCanvasEvent));
    ev->canvas = canvas;
    dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_REFILL, (int)DVZ_RUN_CANVAS_TO_REFILL, ev);
}



static void _enqueue_refill(DvzRun* run, DvzCanvas* canvas, DvzCommands* cmds, uint32_t cmd_idx)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    log_debug("enqueue refill #%d", cmd_idx);

    DvzCanvasEventRefill* ev = calloc(1, sizeof(DvzCanvasEventRefill));
    ev->canvas = canvas;
    ev->cmds = cmds;
    ev->cmd_idx = cmd_idx;
    dvz_deq_enqueue_first(&run->deq, DVZ_RUN_DEQ_REFILL, (int)DVZ_RUN_CANVAS_REFILL, ev);
}



static uint32_t _enqueue_frames(DvzRun* run)
{
    ASSERT(run != NULL);

    DvzApp* app = run->app;
    ASSERT(app != NULL);

    // Go through all canvases.
    uint32_t n_canvas_running = 0;
    DvzCanvas* canvas = NULL;
    DvzContainerIterator iter = dvz_container_iterator(&app->canvases);
    while (iter.item != NULL)
    {
        canvas = (DvzCanvas*)iter.item;
        ASSERT(canvas != NULL);
        ASSERT(canvas->obj.type == DVZ_OBJECT_TYPE_CANVAS);

        // NOTE: Canvas is active iff its status is created, and it has the "running" flag.
        // In that case, enqueue a FRAME event for that canvas.
        if (dvz_obj_is_created(&canvas->obj) && canvas->running)
        {
            _enqueue_canvas_frame(run, canvas);
            n_canvas_running++;
        }

        // NOTE: enqueue a REFILL event at the first frame.
        if (canvas->frame_idx == 0)
        {
            log_debug("refill canvas because frame #0");
            _enqueue_to_refill(run, canvas);
        }

        dvz_container_iter(&iter);
    }

    return n_canvas_running;
}



/*************************************************************************************************/
/*  Utils for the run module                                                                     */
/*************************************************************************************************/

static void blank_commands(DvzCanvas* canvas, DvzCommands* cmds, uint32_t cmd_idx)
{
    dvz_cmd_begin(cmds, cmd_idx);
    dvz_cmd_begin_renderpass(
        cmds, cmd_idx, &canvas->render.renderpass, &canvas->render.framebuffers);
    dvz_cmd_end_renderpass(cmds, cmd_idx);
    dvz_cmd_end(cmds, cmd_idx);
}



static bool _canvas_check(DvzCanvas* canvas)
{
    if (!dvz_obj_is_created(&canvas->obj))
    {
        log_debug("skip canvas frame because canvas is invalid");
        return false;
    }
    return true;
}



// backend-specific
static void _canvas_frame(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);

    // log_trace("canvas frame #%d", canvas->frame_idx);

    DvzApp* app = canvas->app;
    ASSERT(app != NULL);

    // Process only created canvas.
    if (!_canvas_check(canvas))
        return;

    // Poll events.
    if (canvas->window != NULL)
        dvz_window_poll_events(canvas->window);

    // Raise TO_CLOSE if needed.
    if (backend_window_should_close(app->backend, canvas->window->backend_window))
    {
        _enqueue_canvas_event(run, canvas, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_DELETE);
        return;
    }

    // NOTE: swapchain image acquisition happens here

    // We acquire the next swapchain image.
    // NOTE: this call modifies swapchain->img_idx
    if (!canvas->offscreen)
        dvz_swapchain_acquire(
            &canvas->render.swapchain, &canvas->sync.sem_img_available, //
            canvas->cur_frame, NULL, 0);

    // Wait for fence.
    dvz_fences_wait(&canvas->sync.fences_flight, canvas->render.swapchain.img_idx);

    // If there is a problem with swapchain image acquisition, wait and try again later.
    if (canvas->render.swapchain.obj.status == DVZ_OBJECT_STATUS_INVALID)
    {
        log_trace("swapchain image acquisition failed, waiting and skipping this frame");
        dvz_gpu_wait(canvas->gpu);
        return;
    }

    // If the swapchain needs to be recreated (for example, after a resize), do it.
    if (canvas->render.swapchain.obj.status == DVZ_OBJECT_STATUS_NEED_RECREATE)
    {
        log_trace("swapchain image acquisition failed, enqueing a RECREATE task");
        _enqueue_canvas_event(run, canvas, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_RECREATE);
        return;
    }

    // If all good, enqueue a PRESENT task for that canvas.
    // log_info("enqueue present");
    _enqueue_canvas_event(run, canvas, DVZ_RUN_DEQ_PRESENT, DVZ_RUN_CANVAS_PRESENT);

    canvas->frame_idx++;
}



static void _canvas_refill(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);
    ASSERT(canvas->app->run != NULL);

    if (!_canvas_check(canvas))
        return;
    log_debug("canvas refill");

    // TODO: more than 1 DvzCommands (the default, render cmd)
    // TODO OPTIM: this is slow: blocking the GPU for recording the command buffers.
    // Might be better to create new command buffers from the pool??
    dvz_queue_wait(canvas->gpu, DVZ_DEFAULT_QUEUE_RENDER);

    // Reset all command buffers before calling the REFILL callbacks.
    for (int32_t i = (int32_t)canvas->render.swapchain.img_count - 1; i >= 0; i--)
    {
        dvz_cmd_reset(&canvas->cmds_render, (uint32_t)i);
        // blank_commands(canvas, &canvas->cmds_render, i);
        _enqueue_refill(canvas->app->run, canvas, &canvas->cmds_render, (uint32_t)i);
    }
}



// Submit the command buffers, + swapchain synchronization + presentation if not offscreen.
static void _canvas_render(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);

    DvzSubmit* s = &canvas->render.submit;
    uint32_t f = canvas->cur_frame;
    uint32_t img_idx = canvas->render.swapchain.img_idx;

    // Keep track of the fence associated to the current swapchain image.
    dvz_fences_copy(
        &canvas->sync.fences_render_finished, f, //
        &canvas->sync.fences_flight, img_idx);

    // Reset the Submit instance before adding the command buffers.
    dvz_submit_reset(s);

    // Render command buffers empty? Fill them with blank color by default.
    if (canvas->cmds_render.obj.status != DVZ_OBJECT_STATUS_CREATED)
    {
        log_debug("empty command buffers, filling with blank color");
        for (uint32_t i = 0; i < canvas->render.swapchain.img_count; i++)
            blank_commands(canvas, &canvas->cmds_render, i);
    }
    ASSERT(canvas->cmds_render.obj.status == DVZ_OBJECT_STATUS_CREATED);
    // Add the command buffers to the submit instance.
    dvz_submit_commands(s, &canvas->cmds_render);

    if (s->commands_count == 0)
    {
        log_error("no recorded command buffers");
        return;
    }

    if (!canvas->offscreen)
    {
        dvz_submit_wait_semaphores(
            s, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //
            &canvas->sync.sem_img_available, f);

        // Once the render is finished, we signal another semaphore.
        dvz_submit_signal_semaphores(s, &canvas->sync.sem_render_finished, f);
    }

    // SEND callbacks and send the Submit instance.
    // Call PRE_SEND callbacks
    // _event_presend(canvas);

    if (canvas->offscreen)
        ASSERT(img_idx == 0);

    // Send the Submit instance.
    dvz_submit_send(s, img_idx, &canvas->sync.fences_render_finished, f);

    // Call POST_SEND callbacks
    // _event_postsend(canvas);

    // Once the image is rendered, we present the swapchain image.
    // The semaphore used for waiting during presentation may be changed by the canvas
    // callbacks.
    if (!canvas->offscreen)
    {
        dvz_swapchain_present(
            &canvas->render.swapchain, 1, //
            canvas->sync.present_semaphores,
            CLIP(f, 0, canvas->sync.present_semaphores->count - 1));
    }

    canvas->cur_frame = (f + 1) % canvas->sync.fences_render_finished.count;
}



#ifdef __cplusplus
}
#endif

#endif
