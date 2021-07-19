#include "../include/datoviz/run.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_RUN_DEQ_FRAME  0
#define DVZ_RUN_DEQ_MAIN   1
#define DVZ_RUN_DEQ_REFILL 2

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



/*************************************************************************************************/
/*  Run creation                                                                                 */
/*************************************************************************************************/

DvzRun* dvz_run(DvzApp* app)
{
    ASSERT(app != NULL); //

    DvzRun* run = calloc(1, sizeof(DvzRun)); // will be FREE-ed by dvz_run_destroy();
    run->app = app;
    app->run = run;

    // Deq with 3 queues, 1 proc with depth-first dequeue strategy
    run->deq = dvz_deq(3);
    dvz_deq_proc(
        &run->deq, 0, 3, (uint32_t[]){DVZ_RUN_DEQ_FRAME, DVZ_RUN_DEQ_MAIN, DVZ_RUN_DEQ_REFILL});
    dvz_deq_strategy(&run->deq, 0, DVZ_DEQ_STRATEGY_DEPTH_FIRST);

    run->state = DVZ_RUN_STATE_PAUSED;

    return run;
}



/*************************************************************************************************/
/*  Run event loop                                                                               */
/*************************************************************************************************/

static void _enqueue_frame(DvzRun* run, DvzCanvas* canvas)
{
    ASSERT(run != NULL);
    ASSERT(canvas != NULL);

    DvzRunCanvasFrame* ev = calloc(1, sizeof(DvzRunCanvasFrame));
    ev->canvas = canvas;
    ev->frame_idx = ev->canvas->frame_idx;
    dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_FRAME, DVZ_RUN_CANVAS_FRAME, ev);
}



int dvz_run_frame(DvzRun* run)
{
    ASSERT(run != NULL);

    DvzApp* app = run->app;
    ASSERT(app != NULL);

    // Go through all canvases.
    DvzCanvas* canvas = NULL;
    DvzContainerIterator iter = dvz_container_iterator(&app->canvases);
    while (iter.item != NULL)
    {
        canvas = (DvzCanvas*)iter.item;
        ASSERT(canvas != NULL);
        ASSERT(canvas->obj.type == DVZ_OBJECT_TYPE_CANVAS);

        // If the canvas is active, enqueue a FRAME event for that canvas.
        if (canvas->running)
            _enqueue_frame(run, canvas);

        dvz_container_iter(&iter);
    }

    // to decide: one Deq per canvas, or one for the Run?
    // dequeue all items until all queues are empty (depth first dequeue)
    //     first, it may dequeue a FRAME item, for which the callbacks will be called immediately
    //         the FRAME callbacks may enqueue REFILL items (third queue) or
    //         ADD/REMOVE/VISIBLE/ACTIVE items (second queue) they may also enqueue TRANSFER items,
    //         to be processed directly in the background transfer thread, but COPY transfers may
    //         be enqueued, to be handled separately later in the run_frame()
    //     then, it may dequeue MAIN items
    //         the ADD/VISIBLE/ACTIVE/RESIZE callbacks may be called
    //     finally, it may dequeue a REFILL item, which will call the REFILL callbacks
    //         it is up to the REFILL callback to either update the current swapchain cmd buf, or
    //         to queue_wait on the RENDER GPU queue, and update all cmd bufs
    // dequeue the pending COPY transfers in the GPU's context associated to the canvas
    //     the COPY callback
    // swapchain presentation if not offscreen
    // to do : need to create another context queue LIFECYCLE for new/delete/modify of
    // buffer/texture

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

    // NOTE: there is the global frame index for the event loop, but every frame has its own local
    // frame index too.
    for (uint64_t global_frame_idx = 0; global_frame_idx < n; global_frame_idx++)
    {
        ret = dvz_run_frame(run);

        // Stop the event loop if the return code of dvz_run_frame() requires it.
        if (ret == DVZ_RUN_FRAME_RETURN_STOP)
            break;
    }

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
