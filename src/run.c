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



static void _run_flush(DvzRun* run)
{
    ASSERT(run != NULL);
    ASSERT(run->app != NULL);

    log_debug("flush run instance");

    backend_poll_events(run->app->backend, NULL);

    // Flush all queues.
    for (uint32_t i = 0; i < 4; i++)
    {
        log_debug("flush deq #%d", i);
        dvz_deq_dequeue_batch(&run->deq, i);
    }
    // for (uint32_t i = 0; i < 4; i++)
    //     dvz_deq_wait(&run->deq, i);

    dvz_app_wait(run->app);
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


static void _callback_transfers(DvzDeq* deq, void* item, void* user_data)
{
    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);

    DvzCanvasEventFrame* ev = (DvzCanvasEventFrame*)item;
    ASSERT(item != NULL);

    DvzCanvas* canvas = ev->canvas;
    ASSERT(canvas != NULL);

    uint32_t img_idx = canvas->render.swapchain.img_idx;

    DvzGpu* gpu = NULL;

    DvzContainerIterator iter = dvz_container_iterator(&app->gpus);
    while (iter.item != NULL)
    {
        gpu = (DvzGpu*)iter.item;
        ASSERT(gpu != NULL);
        ASSERT(gpu->obj.type == DVZ_OBJECT_TYPE_GPU);
        if (!dvz_obj_is_created(&gpu->obj))
            break;
        ASSERT(gpu->context != NULL);

        // Process the pending data transfers (copies and dup transfers that require
        // synchronization and integration with the event loop).
        dvz_transfers_frame(&gpu->context->transfers, img_idx);

        dvz_container_iter(&iter);
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
    _run_flush(app->run);

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
    ASSERT(ev->canvas != NULL);
    // _canvas_refill(ev->canvas);

    // Unblock all command buffers so that they are refilled one by one at the next frames.
    memset(ev->canvas->cmds_render.blocked, 0, sizeof(ev->canvas->cmds_render.blocked));

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



static void _callback_upfill(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    log_trace("callback to refill");

    DvzCanvasEventUpfill* ev = (DvzCanvasEventUpfill*)item;
    ASSERT(ev != NULL);

    DvzApp* app = (DvzApp*)user_data;
    ASSERT(app != NULL);
    ASSERT(app->run != NULL);

    ASSERT(ev->canvas != NULL);
    ASSERT(ev->canvas->gpu != NULL);
    ASSERT(ev->dat != NULL);
    ASSERT(ev->data != NULL);
    ASSERT(ev->size > 0);

    // Stop rendering.
    dvz_queue_wait(ev->canvas->gpu, DVZ_DEFAULT_QUEUE_RENDER);

    // Upload the data and wait.
    DvzContext* ctx = ev->canvas->gpu->context;
    ASSERT(ctx != NULL);
    dvz_dat_upload(ev->dat, ev->offset, ev->size, ev->data, true);

    // Enqueue to refill, which will trigger refill in the current frame (in the REFILL dequeue
    // just after the MAIN dequeue which called the current function).
    _enqueue_to_refill(app->run, ev->canvas);
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

    // Submit the command buffers and make the swapchain rendering.
    canvas_render(canvas);
}



/*************************************************************************************************/
/*  Run creation                                                                                 */
/*************************************************************************************************/

static void _default_refill(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(item != NULL);
    ASSERT(user_data != NULL);
    DvzCanvasEventRefill* ev = (DvzCanvasEventRefill*)item;

    DvzCanvas* canvas = ev->canvas;
    ASSERT(canvas != NULL);

    // Blank canvas by default.
    uint32_t img_idx = canvas->render.swapchain.img_idx;
    log_debug("default command buffer refill with blank canvas for image #%d", img_idx);
    DvzCommands* cmds = &canvas->cmds_render;
    blank_commands(canvas, cmds, img_idx);
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

    // Deq batch callbacks.
    dvz_deq_proc_batch_callback(
        &run->deq, DVZ_RUN_DEQ_FRAME, (int)DVZ_RUN_CANVAS_FRAME, _callback_frame, app);

    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_REFILL, (int)DVZ_RUN_CANVAS_TO_REFILL, _callback_to_refill, app);

    // Default refill callback.
    // NOTE: this is a default callback: it will be discarded if the user registers other command
    // buffer refill callbacks.
    dvz_deq_callback_default(
        &run->deq, DVZ_RUN_DEQ_REFILL, (int)DVZ_RUN_CANVAS_REFILL, _default_refill, app);

    // Main callbacks.

    // New canvas.
    dvz_deq_callback(&run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_NEW, _callback_new, app);

    // Delete canvas.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_DELETE, _callback_delete, app);

    // Clear color.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_CLEAR_COLOR, _callback_clear_color, app);

    // Recreate.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_RECREATE, _callback_recreate, app);

    // Upfill.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_UPFILL, _callback_upfill, app);

    // Call dvz_transfers_frame() in the main thread, at every frame, with the current canvas
    // swapchain image index.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_MAIN, (int)DVZ_RUN_CANVAS_FRAME, _callback_transfers, app);

    // Present callbacks.
    dvz_deq_callback(
        &run->deq, DVZ_RUN_DEQ_PRESENT, (int)DVZ_RUN_CANVAS_PRESENT, _callback_present, app);

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
        // NOTE: pending data transfers (copies and dup transfers) happen here, in a FRAME callback
        // in the MAIN queue (main thread).
        log_trace("dequeue batch main");
        dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_MAIN);

        // Refill canvases if needed.
        log_trace("dequeue batch refill");
        dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_REFILL);
    }

    // Swapchain presentation.
    log_trace("dequeue batch present");
    dvz_deq_dequeue_batch(&run->deq, DVZ_RUN_DEQ_PRESENT);

    _gpu_sync_hack(app);

    // If no canvas is running, stop the event loop.
    if (n_canvas_running == 0)
        return DVZ_RUN_FRAME_RETURN_STOP;

    return 0;
}



/*************************************************************************************************/
/*  Dat upfill                                                                                   */
/*************************************************************************************************/

void dvz_dat_upfill(
    DvzRun* run, DvzCanvas* canvas, DvzDat* dat, VkDeviceSize offset, VkDeviceSize size,
    void* data)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);
    ASSERT(canvas->gpu != NULL);
    ASSERT(dat != NULL);
    ASSERT(data != NULL);
    ASSERT(size > 0);

    // Stop rendering.
    dvz_queue_wait(canvas->gpu, DVZ_DEFAULT_QUEUE_RENDER);

    // Upload the data and wait.
    DvzContext* ctx = canvas->gpu->context;
    ASSERT(ctx != NULL);
    dvz_dat_upload(dat, offset, size, data, true);

    // Enqueue to refill, which will trigger refill in the current frame (in the REFILL dequeue
    // just after the MAIN dequeue which called the current function).
    _enqueue_to_refill(run, canvas);
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
    _run_flush(run);

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
    log_debug("destroy run instance");
    // run->destroying = true;

    // Wait.
    _run_flush(app->run);

    // Block the input of all canvases.
    {
        DvzContainerIterator iterator = dvz_container_iterator(&app->canvases);
        DvzCanvas* canvas = NULL;
        while (iterator.item != NULL)
        {
            canvas = iterator.item;
            ASSERT(canvas != NULL);
            if (!dvz_obj_is_created(&canvas->obj))
                break;

            dvz_input_block(&canvas->input, true);
            dvz_container_iter(&iterator);
        }
    }

    dvz_deq_destroy(&run->deq);

    FREE(run);
    app->run = NULL;
}
