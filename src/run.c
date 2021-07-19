#include "../include/datoviz/run.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_RUN_DEQ_FRAME  0
#define DVZ_RUN_DEQ_MAIN   1
#define DVZ_RUN_DEQ_REFILL 2

#define DVZ_RUN_DEFAULT_FRAME_COUNT 0



/*************************************************************************************************/
/*  Runner                                                                                       */
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



int dvz_run_frame(DvzRun* run)
{
    ASSERT(run != NULL);

    return 0;
}



int dvz_run_loop(DvzRun* run, uint64_t frame_count)
{
    ASSERT(run != NULL);

    for (uint64_t frame_idx = 0; frame_idx < frame_count || UINT64_MAX; frame_idx++)
    {
        dvz_run_frame(run);
    }

    return 0;
}



void dvz_run_setup(DvzRun* run, uint64_t frame_count, bool offscreen, const char* filepath)
{
    ASSERT(run != NULL); //
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

    // Enable the autorun?
    DvzAutorun empty = {0};
    // Enable if and only if at least one of the autorun fields is not blank.
    autorun->enable = memcmp(autorun, &empty, sizeof(DvzAutorun)) != 0;
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
