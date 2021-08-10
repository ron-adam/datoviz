#ifndef DVZ_RUN_UTILS_HEADER
#define DVZ_RUN_UTILS_HEADER

#include "../include/datoviz/run.h"
#include "../include/datoviz/vklite.h"
#include "canvas_utils.h"
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



static void _enqueue_canvas_frame(DvzRun* run, DvzCanvas* canvas, uint32_t q_idx)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    // log_info("enqueue frame");

    DvzCanvasEventFrame* ev = calloc(1, sizeof(DvzCanvasEventFrame));
    ev->canvas = canvas;
    ev->frame_idx = ev->canvas->frame_idx;
    dvz_deq_enqueue(&run->deq, q_idx, (int)DVZ_RUN_CANVAS_FRAME, ev);
}



static void _enqueue_to_refill(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);

    DvzCanvasEvent* ev = calloc(1, sizeof(DvzCanvasEvent));
    ev->canvas = canvas;
    dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_REFILL, (int)DVZ_RUN_CANVAS_TO_REFILL, ev);
}



static void _enqueue_refill(DvzRun* run, DvzCanvas* canvas, DvzCommands* cmds, uint32_t cmd_idx)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    // log_debug("enqueue refill #%d", cmd_idx);

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
            _enqueue_canvas_frame(run, canvas, DVZ_RUN_DEQ_FRAME);
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

static bool _canvas_check(DvzCanvas* canvas)
{
    if (!dvz_obj_is_created(&canvas->obj))
    {
        log_debug("skip canvas frame because canvas is invalid");
        return false;
    }
    return true;
}



// Perform the actual user-specified command buffer refill, for the current swapchain image only.
static void _canvas_refill(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);

    DvzRun* run = canvas->app->run;
    ASSERT(run != NULL);

    if (!_canvas_check(canvas))
        return;

    // Default command buffers for the canvas.
    DvzCommands* cmds = &canvas->cmds_render;
    uint32_t img_idx = canvas->render.swapchain.img_idx;

    // Check if the command buffer for the current swapchain image is blocked.
    if (cmds->blocked[img_idx])
        return;
    log_debug("canvas refill #%d", img_idx);

    // HERE we assume the command buffer is NOT blocked, so we want to refill it immediately by
    // calling the user callback. But first, we need to reset it.
    dvz_cmd_reset(cmds, img_idx);

    // HACK: here, we want to call the user callback to REFILL directly. We need to enqueue first
    // and dequeue immediately to be sure the callback is called and the command buffer is filled.
    // Better to have a special deq method to call the callback and bypassing the queue altogether/
    _enqueue_refill(run, canvas, cmds, img_idx);
    // This will call the user callback for REFILL, for the current swapchain image.
    dvz_deq_dequeue(&run->deq, DVZ_RUN_DEQ_REFILL, true);

    // Make sure the command buffer is filled.
    // NOTE: there is a default callback registered for the REFILL event, which fills the canvas
    // with a blank color. If there are other FILL callbacks, this default callback is discarded.
    ASSERT(dvz_obj_is_created(&cmds->obj));

    // Immediately block the command buffer so that it is not refilled at every frame if that's not
    // useful.
    cmds->blocked[img_idx] = true;
}



// backend-specific
static void _canvas_frame(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);

    log_trace("canvas frame #%d", canvas->frame_idx);

    DvzApp* app = canvas->app;
    ASSERT(app != NULL);

    // Process only created canvas.
    if (!_canvas_check(canvas))
        return;

    // Poll events.
    if (canvas->window != NULL)
        dvz_window_poll_events(canvas->window);

    // Raise TO_CLOSE if needed.
    void* backend_window = canvas->window != NULL ? canvas->window->backend_window : NULL;
    if (backend_window_should_close(app->backend, backend_window))
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

    // NOTE: command buffer refill actually happens here.
    _canvas_refill(canvas);

    // If all good, enqueue a PRESENT task for that canvas. The PRESENT callback does the rendering
    // (cmd buf submission) and send the rendered image for presentation to the swapchain.
    // log_info("enqueue present");
    _enqueue_canvas_event(run, canvas, DVZ_RUN_DEQ_PRESENT, DVZ_RUN_CANVAS_PRESENT);

    canvas->frame_idx++;
}



#ifdef __cplusplus
}
#endif

#endif
