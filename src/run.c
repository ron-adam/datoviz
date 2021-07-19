#include "../include/datoviz/run.h"
#include "../include/datoviz/canvas.h"
#include "../include/datoviz/vklite.h"
#include "vklite_utils.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_RUN_DEFAULT_FRAME_COUNT 0

// Return codes for dvz_run_frame()
// 0: the frame ran successfully
// -1: an error occurred, need to continue the loop as normally as possible
// 1: need to stop the run
#define DVZ_RUN_FRAME_RETURN_OK    0
#define DVZ_RUN_FRAME_RETURN_ERROR -1
#define DVZ_RUN_FRAME_RETURN_STOP  1



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

static bool _autorun_is_set(DvzAutorun* autorun)
{
    ASSERT(autorun != NULL);
    // Enable the autorun?
    DvzAutorun empty = {0};
    // Enable if and only if at least one of the autorun fields is not blank.
    return memcmp(autorun, &empty, sizeof(DvzAutorun)) != 0;
}



static void _autorun_launch(DvzRun* run)
{
    ASSERT(run != NULL);
    ASSERT(run->autorun.enable);
    DvzAutorun* ar = &run->autorun;
    log_debug(
        "start autorun: offscreen %d, frames %d, save %s", //
        ar->frame_count, ar->frame_count, ar->filepath);

    // TODO: implement autorun.
}



static void blank_commands(DvzCanvas* canvas, DvzCommands* cmds, uint32_t cmd_idx)
{
    dvz_cmd_begin(cmds, cmd_idx);
    dvz_cmd_begin_renderpass(
        cmds, cmd_idx, &canvas->render.renderpass, &canvas->render.framebuffers);
    dvz_cmd_end_renderpass(cmds, cmd_idx);
    dvz_cmd_end(cmds, cmd_idx);
}



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
    dvz_deq_enqueue(&run->deq, deq_idx, type, ev);
}



static void _enqueue_canvas_frame(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    // log_info("enqueue frame");

    DvzCanvasEventFrame* ev = calloc(1, sizeof(DvzCanvasEventFrame));
    ev->canvas = canvas;
    ev->frame_idx = ev->canvas->frame_idx;
    dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_FRAME, DVZ_RUN_CANVAS_FRAME, ev);
}



static void _enqueue_refill(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    // log_info("enqueue refill");

    DvzCanvasEvent* ev = calloc(1, sizeof(DvzCanvasEvent));
    ev->canvas = canvas;
    dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_REFILL, DVZ_RUN_CANVAS_REFILL, ev);
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
            log_info("refill canvas because frame #0");
            _enqueue_refill(run, canvas);
        }

        dvz_container_iter(&iter);
    }

    return n_canvas_running;
}



/*************************************************************************************************/
/*  Canvas callbacks                                                                             */
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



// backend-specific
static void _canvas_frame(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);

    log_info("canvas frame #%d", canvas->frame_idx);

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

    if (!_canvas_check(canvas))
        return;
    log_debug("canvas refill");

    // TODO: more than 1 DvzCommands (the default, render cmd)
    // TODO OPTIM: this is slow: blocking the GPU for recording the command buffers.
    // Might be better to create new command buffers from the pool??
    dvz_queue_wait(canvas->gpu, DVZ_DEFAULT_QUEUE_RENDER);

    // Reset all command buffers before calling the REFILL callbacks.
    for (uint32_t i = 0; i < canvas->render.swapchain.img_count; i++)
    {
        dvz_cmd_reset(&canvas->cmds_render, i);
        blank_commands(canvas, &canvas->cmds_render, i);
    }
}



static void _callback_new(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    log_debug("create new canvas");

    ASSERT(deq != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);

    DvzCanvasEventNew* ev = (DvzCanvasEventNew*)item;
    ASSERT(ev != NULL);

    // Create the canvas.
    DvzCanvas* canvas = dvz_canvas(ev->gpu, ev->width, ev->height, ev->flags);
    dvz_canvas_create(canvas);
}



static void _callback_frame(
    DvzDeq* deq, DvzDeqProcBatchPosition pos, uint32_t item_count, DvzDeqItem* items,
    void* user_data)
{
    ASSERT(deq != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);
    DvzRun* run = app->run;
    ASSERT(run != NULL);

    DvzCanvasEventFrame* ev = NULL;
    for (uint32_t i = 0; i < item_count; i++)
    {
        ASSERT(items[i].type == DVZ_RUN_CANVAS_FRAME);
        ev = items[i].item;
        ASSERT(ev != NULL);

        // TODO: optim: if multiple FRAME events for 1 canvas, make sure we call it only once.
        // One frame for one canvas.
        _canvas_frame(run, ev->canvas);
    }
}



static void _callback_recreate(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    log_debug("canvas recreate");

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);
    DvzRun* run = app->run;
    ASSERT(run != NULL);

    DvzCanvasEvent* ev = (DvzCanvasEvent*)item;
    ASSERT(item != NULL);
    DvzCanvas* canvas = ev->canvas;

    // Recreate the canvas.
    dvz_canvas_recreate(canvas);

    // Enqueue a REFILL after the canvas recreation.
    _enqueue_refill(run, canvas);
}



static void _callback_delete(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);

    DvzCanvasEvent* ev = (DvzCanvasEvent*)item;
    ASSERT(ev != NULL);
    DvzCanvas* canvas = ev->canvas;

    if (!_canvas_check(canvas))
        return;
    log_debug("delete canvas");

    // Wait for all GPUs to be idle.
    dvz_app_wait(app);

    // Destroy the canvas.
    dvz_canvas_destroy(canvas);
}



static void _callback_clear_color(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);

    DvzCanvasEventClearColor* ev = (DvzCanvasEventClearColor*)item;
    ASSERT(ev != NULL);
    DvzCanvas* canvas = ev->canvas;
    if (!_canvas_check(canvas))
        return;
    log_debug("change canvas clear color");

    canvas->render.renderpass.clear_values->color = (VkClearColorValue){{ev->r, ev->g, ev->b, 1}};
    _enqueue_refill(app->run, canvas);
}



static void _callback_refill(
    DvzDeq* deq, DvzDeqProcBatchPosition pos, uint32_t item_count, DvzDeqItem* items,
    void* user_data)
{
    ASSERT(deq != NULL);

    // gathers the cmd buf idx of all dequeued REFILL tasks and, for each of these cmd buf idx,
    // fills the cmd buf for each canvas, check if a cmd buf has been filled, and if not, fill
    // it with blank cmd buf

    DvzCanvasEvent* ev = NULL;
    for (uint32_t i = 0; i < item_count; i++)
    {
        ASSERT(items[i].type == DVZ_RUN_CANVAS_REFILL);
        ev = items[i].item;
        ASSERT(ev != NULL);

        // TODO: optim: if multiple FRAME events for 1 canvas, make sure we call it only once.
        // One frame for one canvas.
        _canvas_refill(ev->canvas);
    }
}



// backend-specific
static void _callback_present(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);

    // frame submission for that canvas: submit cmd bufs, present swapchain

    ASSERT(deq != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);
    // DvzRun* run = app->run;
    // ASSERT(run != NULL);

    DvzCanvasEvent* ev = (DvzCanvasEvent*)item;
    ASSERT(item != NULL);
    DvzCanvas* canvas = ev->canvas;

    // Process only created canvas.
    if (!_canvas_check(canvas))
        return;
    // log_debug("present canvas");

    DvzSubmit* s = &canvas->render.submit;
    uint32_t f = canvas->cur_frame;
    uint32_t img_idx = canvas->render.swapchain.img_idx;

    // Keep track of the fence associated to the current swapchain image.
    dvz_fences_copy(
        &canvas->sync.fences_render_finished, f, //
        &canvas->sync.fences_flight, img_idx);

    // Reset the Submit instance before adding the command buffers.
    dvz_submit_reset(s);

    // Add the command buffers to the submit instance.
    // Default render commands.
    if (canvas->cmds_render.obj.status == DVZ_OBJECT_STATUS_CREATED)
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
    {
        // Call PRE_SEND callbacks
        // _event_presend(canvas);

        // Send the Submit instance.
        dvz_submit_send(s, img_idx, &canvas->sync.fences_render_finished, f);

        // Call POST_SEND callbacks
        // _event_postsend(canvas);
    }

    // Once the image is rendered, we present the swapchain image.
    // The semaphore used for waiting during presentation may be changed by the canvas
    // callbacks.
    if (!canvas->offscreen)
        dvz_swapchain_present(
            &canvas->render.swapchain, 1, //
            canvas->sync.present_semaphores,
            CLIP(f, 0, canvas->sync.present_semaphores->count - 1));

    canvas->cur_frame = (f + 1) % canvas->sync.fences_render_finished.count;
}



static void _callback_copy(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    // TODO
    // _callback_copy()
    //     creates new copy cmd buf and fills it in
}



static void _callback_copy_batch(
    DvzDeq* deq, DvzDeqProcBatchPosition pos, uint32_t item_count, DvzDeqItem* items,
    void* user_data)
{
    ASSERT(deq != NULL);
    // TODO
    // _callback_copy_batch()
    //     waits on RENDER GPU queue, then submit all cmd bufs (stored in the dequeued items), and
    //     wait for the TRANSFER GPU queue
}



/*************************************************************************************************/
/*  Run creation                                                                                 */
/*************************************************************************************************/

DvzRun* dvz_run(DvzApp* app)
{
    ASSERT(app != NULL); //

    DvzRun* run = calloc(1, sizeof(DvzRun)); // will be FREE-ed by dvz_run_destroy();
    run->app = app;
    app->run = run;

    // Deq with 4 queues: FRAME, MAIN, REFILL, PRESENT
    run->deq = dvz_deq(4);
    dvz_deq_proc(&run->deq, 0, 1, (uint32_t[]){DVZ_RUN_DEQ_FRAME});
    dvz_deq_proc(&run->deq, 1, 1, (uint32_t[]){DVZ_RUN_DEQ_MAIN});
    dvz_deq_proc(&run->deq, 2, 1, (uint32_t[]){DVZ_RUN_DEQ_REFILL});
    dvz_deq_proc(&run->deq, 3, 1, (uint32_t[]){DVZ_RUN_DEQ_PRESENT});
    // dvz_deq_strategy(&run->deq, 0, DVZ_DEQ_STRATEGY_DEPTH_FIRST);

    // Deq batch callbacks.
    dvz_deq_proc_batch_callback(
        &run->deq, DVZ_RUN_DEQ_FRAME, DVZ_RUN_CANVAS_FRAME, _callback_frame, app);

    // TODO: let the user do this, and only register a refill callback if there is none.
    // Alternatively, only allow 1 callback??
    dvz_deq_proc_batch_callback(
        &run->deq, DVZ_RUN_DEQ_REFILL, DVZ_DEQ_PROC_CALLBACK_POST, _callback_refill, app);

    // Main callbacks.
    dvz_deq_callback(&run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_NEW, _callback_new, app);

    dvz_deq_callback(&run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_DELETE, _callback_delete, app);

    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_CLEAR_COLOR, _callback_clear_color, app);

    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_RECREATE, _callback_recreate, app);

    // Present callbacks.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_PRESENT, DVZ_RUN_CANVAS_PRESENT, _callback_present, app);

    run->state = DVZ_RUN_STATE_PAUSED;

    return run;
}



/*************************************************************************************************/
/*  Run event loop                                                                               */
/*************************************************************************************************/

static void _dequeue_copies(DvzApp* app)
{
    ASSERT(app != NULL);
    DvzGpu* gpu = NULL;
    DvzContainerIterator iter = dvz_container_iterator(&app->gpus);
    while (iter.item != NULL)
    {
        gpu = (DvzGpu*)iter.item;
        ASSERT(gpu != NULL);
        ASSERT(gpu->obj.type == DVZ_OBJECT_TYPE_GPU);

        // Process all copies with hard GPU synchronization before and after, as this is a sensible
        // operation (we write to GPU data that is likely to be used when rendering).
        if (gpu->context != NULL)
            dvz_deq_dequeue(&gpu->context->deq, DVZ_TRANSFER_PROC_CPY, false);

        dvz_container_iter(&iter);
    }
}



static void _gpu_sync_hack(DvzApp* app)
{
    // BIG HACK: this call is required at every frame, otherwise the event loop randomly crashes,
    // fences deadlock (?) and the Vulkan validation layers raise errors, causing the whole system
    // to crash for ~20 seconds. This is probably a ugly hack and I'd appreciate any help from a
    // Vulkan synchronization expert.

    // NOTE: this has never been tested with multiple GPUs yet.
    DvzContainerIterator iterator = dvz_container_iterator(&app->gpus);
    DvzGpu* gpu = NULL;
    while (iterator.item != NULL)
    {
        gpu = iterator.item;
        if (!dvz_obj_is_created(&gpu->obj))
            break;

        // Pending transfers.
        ASSERT(gpu->context != NULL);
        // NOTE: the function below uses hard GPU synchronization primitives

        // IMPORTANT: we need to wait for the present queue to be idle, otherwise the GPU hangs
        // when waiting for fences (not sure why). The problem only arises when using different
        // queues for command buffer submission and swapchain present. There has be a better
        // way to fix this.
        if (gpu->queues.queues[DVZ_DEFAULT_QUEUE_PRESENT] != VK_NULL_HANDLE &&
            gpu->queues.queues[DVZ_DEFAULT_QUEUE_PRESENT] !=
                gpu->queues.queues[DVZ_DEFAULT_QUEUE_RENDER])
        {
            dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_PRESENT);
        }

        dvz_container_iter(&iterator);
    }
}



// Run one frame for all active canvases, process all MAIN events, and perform all pending data
// copies.
int dvz_run_frame(DvzRun* run)
{
    ASSERT(run != NULL);

    DvzApp* app = run->app;
    ASSERT(app != NULL);

    // Go through all canvases to find out which are active, and enqueue a FRAME event for them.
    uint32_t n_canvas_running = _enqueue_frames(run);

    // TODO: the next calls should be in a loop, until both deqs are empty.

    // Dequeue all items until all queues are empty (depth first dequeue)
    //
    // NOTES: This is the call when most of the logic happens!

    // First, this call may dequeue a FRAME item, for which the callbacks will be called
    // immediately. The FRAME callbacks may enqueue REFILL items (third queue) or
    // ADD/REMOVE/VISIBLE/ACTIVE items (second queue).
    // They may also enqueue TRANSFER items, to be processed directly in the background transfer
    // thread. However, COPY transfers may be enqueued, to be handled separately later in the
    // run_frame().
    dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_FRAME);

    // Then, dequeue MAIN items. The ADD/VISIBLE/ACTIVE/RESIZE callbacks may be called.
    dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_MAIN);

    // Refill canvases if needed.
    dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_REFILL);

    // Swapchain presentation.
    dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_PRESENT);

    // Dequeue the pending COPY transfers in the GPU contexts.
    _dequeue_copies(app);

    _gpu_sync_hack(app);

    // If no canvas is running, stop the event loop.
    if (n_canvas_running == 0)
        return DVZ_RUN_FRAME_RETURN_STOP;

    return 0;
}



/*************************************************************************************************/
/*  Run functions                                                                                */
/*************************************************************************************************/

int dvz_run_loop(DvzRun* run, uint64_t frame_count)
{
    ASSERT(run != NULL);

    int ret = 0;
    uint64_t n = frame_count > 0 ? frame_count : UINT64_MAX;

    run->state = DVZ_RUN_STATE_RUNNING;

    // NOTE: there is the global frame index for the event loop, but every frame has its own local
    // frame index too.
    for (run->global_frame_idx = 0; run->global_frame_idx < n; run->global_frame_idx++)
    {
        log_trace("event loop, global frame #%d", run->global_frame_idx);
        ret = dvz_run_frame(run);

        // Stop the event loop if the return code of dvz_run_frame() requires it.
        if (ret == DVZ_RUN_FRAME_RETURN_STOP)
        {
            log_debug("end event loop");
            break;
        }
    }

    run->state = DVZ_RUN_STATE_PAUSED;

    return 0;
}



void dvz_run_setup(DvzRun* run, uint64_t frame_count, bool offscreen, char* filepath)
{
    ASSERT(run != NULL);

    DvzAutorun* autorun = &run->autorun;
    ASSERT(autorun != NULL);

    autorun->filepath = filepath;
    autorun->offscreen = offscreen;
    autorun->frame_count = frame_count;

    autorun->enable = _autorun_is_set(autorun);
}



void dvz_run_setupenv(DvzRun* run)
{
    ASSERT(run != NULL);

    DvzAutorun* autorun = &run->autorun;
    ASSERT(autorun != NULL);

    char* s = NULL;

    // Offscreen?
    s = getenv("DVZ_RUN_OFFSCREEN");
    if (s)
        autorun->offscreen = true;

    // Number of frames.
    s = getenv("DVZ_RUN_FRAMES");
    if (s)
        autorun->frame_count = strtoull(s, NULL, 10);

    // Screenshot and video.
    s = getenv("DVZ_RUN_SAVE");
    if (s)
        strncpy(autorun->filepath, s, DVZ_PATH_MAX_LEN);

    // Enable the autorun if and only if at least one of the autorun fields is not blank.
    autorun->enable = _autorun_is_set(autorun);
}



int dvz_run_auto(DvzRun* run)
{
    ASSERT(run != NULL);

    dvz_run_setupenv(run);
    if (run->autorun.enable)
    {
        _autorun_launch(run);
    }
    else
    {
        dvz_run_loop(run, DVZ_RUN_DEFAULT_FRAME_COUNT);
    }

    return 0;
}



void dvz_run_destroy(DvzRun* run)
{
    ASSERT(run != NULL);
    dvz_deq_destroy(&run->deq);

    DvzApp* app = run->app;
    ASSERT(app != NULL);

    FREE(run);
    app->run = NULL;
}
