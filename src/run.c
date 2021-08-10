#include "../include/datoviz/run.h"
#include "../include/datoviz/canvas.h"
#include "../include/datoviz/vklite.h"
#include "canvas_utils.h"
#include "run_utils.h"
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



static void _run_wait(DvzRun* run)
{
    ASSERT(run != NULL);
    ASSERT(run->app != NULL);

    backend_poll_events(run->app->backend, NULL);

    // for (uint32_t i = 0; i < 4; i++)
    //     dvz_deq_wait(&run->deq, i);

    dvz_app_wait(run->app);
}



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
            dvz_deq_dequeue(&gpu->context->transfers.deq, DVZ_TRANSFER_PROC_CPY, false);

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



/*************************************************************************************************/
/*  Canvas callbacks                                                                             */
/*************************************************************************************************/

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
    log_trace("callback frame");

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

        // We enqueue another FRAME event, but in the MAIN queue: this is the event the user
        // callbacks will subscribe to.
        _enqueue_canvas_frame(run, ev->canvas, DVZ_RUN_DEQ_MAIN);

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
    _enqueue_to_refill(run, canvas);
}



static void _callback_delete(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);

    DvzCanvasEvent* ev = (DvzCanvasEvent*)item;
    ASSERT(ev != NULL);
    DvzCanvas* canvas = ev->canvas;

    // if (!_canvas_check(canvas))
    //     return;
    log_debug("delete canvas");
    // canvas->destroying = true;
    // canvas->input.destroying = true;

    // Wait before destroying the canvas.
    _run_wait(app->run);

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
    _enqueue_to_refill(app->run, canvas);
}



static void _callback_to_refill(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    log_trace("callback to refill");

    DvzCanvasEvent* ev = (DvzCanvasEvent*)item;
    ASSERT(ev != NULL);
    _canvas_refill(ev->canvas);
    // for (uint32_t i = 0; i < item_count; i++)
    // {
    //     ASSERT(items[i].type == DVZ_RUN_CANVAS_TO_REFILL);
    //     ev = item;
    //     ASSERT(ev != NULL);

    //     // TODO: optim: if multiple REFILL events for 1 canvas, make sure we call it only once.
    //     // One frame for one canvas.
    //     _canvas_refill(ev->canvas);
    // }
}



// To be provided by the user.
// static void _callback_refill(DvzDeq* deq, void* item, void* user_data)
// {
//     ASSERT(deq != NULL);

//     DvzApp* app = (DvzApp*)user_data;
//     ASSERT(app != NULL);

//     DvzCanvasEventRefill* ev = (DvzCanvasEventRefill*)item;
//     ASSERT(ev != NULL);
//     DvzCanvas* canvas = ev->canvas;
//     if (!_canvas_check(canvas))
//         return;
//     log_debug("refill canvas");
//     // TODO: refill cmd buf
// }


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

    // Submit the command buffers and make the swapchain rendering.
    canvas_render(canvas);
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

static void _refill_callback_wait(
    DvzDeq* deq, DvzDeqProcBatchPosition pos, uint32_t item_count, DvzDeqItem* items,
    void* user_data)
{
    if (item_count == 0)
        return;

    ASSERT(deq != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);

    // HACK: full wait before filling the command buffers.
    dvz_app_wait(app);

    // DvzCanvasEventRefill* ev = NULL;

    // ASSERT(item_count > 0);
    // ASSERT(items != NULL);
    // for (uint32_t i = 0; i < item_count; i++)
    // {
    //     log_trace("wait on the render queue before filling the command buffers");
    //     ev = (DvzCanvasEventRefill*)items[i].item;
    //     ASSERT(ev != NULL);
    //     ASSERT(ev->canvas != NULL);
    //     ASSERT(ev->canvas->gpu != NULL);
    //     dvz_queue_wait(ev->canvas->gpu, DVZ_DEFAULT_QUEUE_RENDER);
    // }
}

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
        &run->deq, DVZ_RUN_DEQ_FRAME, (int)DVZ_RUN_CANVAS_FRAME, _callback_frame, app);

    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_REFILL, (int)DVZ_RUN_CANVAS_TO_REFILL, _callback_to_refill, app);

    // Main callbacks.
    dvz_deq_callback(&run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_NEW, _callback_new, app);

    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_DELETE, _callback_delete, app);

    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_CLEAR_COLOR, _callback_clear_color, app);

    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_RECREATE, _callback_recreate, app);

    // Present callbacks.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_PRESENT, (int)DVZ_RUN_CANVAS_PRESENT, _callback_present, app);

    // HACK: callback before the REFILL user callbacks are called, that is used to wait on the
    // relevant GPUs so that we can safely refill the canvas command buffers.
    dvz_deq_proc_batch_callback(
        &run->deq, DVZ_RUN_DEQ_REFILL, (int)DVZ_DEQ_PROC_CALLBACK_PRE, _refill_callback_wait, app);
    run->state = DVZ_RUN_STATE_PAUSED;

    return run;
}



/*************************************************************************************************/
/*  Run event loop                                                                               */
/*************************************************************************************************/

static int _deq_size(DvzRun* run)
{
    ASSERT(run != NULL);
    int size = 0;
    size += dvz_fifo_size(&run->deq.queues[DVZ_RUN_DEQ_FRAME]);
    size += dvz_fifo_size(&run->deq.queues[DVZ_RUN_DEQ_MAIN]);
    size += dvz_fifo_size(&run->deq.queues[DVZ_RUN_DEQ_REFILL]);
    return size;
}

// Run one frame for all active canvases, process all MAIN events, and perform all pending data
// copies.
int dvz_run_frame(DvzRun* run)
{
    ASSERT(run != NULL);

    DvzApp* app = run->app;
    ASSERT(app != NULL);

    log_trace("frame #%06d", run->global_frame_idx);

    // Go through all canvases to find out which are active, and enqueue a FRAME event for them.
    uint32_t n_canvas_running = _enqueue_frames(run);

    // Dequeue all items until all queues are empty (depth first dequeue)
    //
    // NOTES: This is the call when most of the logic happens!
    while (_deq_size(run) > 0)
    {
        // First, this call may dequeue a FRAME item, for which the callbacks will be called
        // immediately. The FRAME callbacks may enqueue REFILL items (third queue) or
        // ADD/REMOVE/VISIBLE/ACTIVE items (second queue).
        // They may also enqueue TRANSFER items, to be processed directly in the background
        // transfer thread. However, COPY transfers may be enqueued, to be handled separately later
        // in the run_frame().
        log_trace("dequeue batch frame");
        dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_FRAME);

        // Then, dequeue MAIN items. The ADD/VISIBLE/ACTIVE/RESIZE callbacks may be called.
        log_trace("dequeue batch main");
        dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_MAIN);

        // Refill canvases if needed.
        log_trace("dequeue batch refill");
        dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_REFILL);
    }

    // Swapchain presentation.
    log_trace("dequeue batch present");
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

    log_debug("run loop with %d frames", frame_count);

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

    // Wait.
    _run_wait(run);

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
    DvzApp* app = run->app;
    ASSERT(app != NULL);
    // run->destroying = true;

    // Wait.
    _run_wait(app->run);

    dvz_deq_destroy(&run->deq);

    FREE(run);
    app->run = NULL;
}
