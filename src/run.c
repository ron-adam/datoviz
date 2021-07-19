#include "../include/datoviz/run.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_RUN_DEQ_FRAME  0
#define DVZ_RUN_DEQ_MAIN   1
#define DVZ_RUN_DEQ_REFILL 2



/*************************************************************************************************/
/*  Runner                                                                                       */
/*************************************************************************************************/

DvzRun dvz_run(DvzApp* app)
{
    ASSERT(app != NULL); //

    DvzRun run = {0};
    // Deq with 3 queues, 1 proc with depth-first dequeue strategy
    run.deq = dvz_deq(3);
    dvz_deq_proc(
        &run.deq, 0, 3, (uint32_t[]){DVZ_RUN_DEQ_FRAME, DVZ_RUN_DEQ_MAIN, DVZ_RUN_DEQ_REFILL});
    dvz_deq_strategy(&run.deq, 0, DVZ_DEQ_STRATEGY_DEPTH_FIRST);

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

    return 0;
}



void dvz_run_setup(DvzRun* run, uint64_t frame_count, bool offscreen, const char* filepath)
{
    ASSERT(run != NULL); //
}



void dvz_run_setupenv(DvzRun* run)
{
    ASSERT(run != NULL); //
}



int dvz_run_auto(DvzRun* run)
{
    ASSERT(run != NULL);

    return 0;
}



void dvz_run_destroy(DvzRun* run)
{
    ASSERT(run != NULL);
    dvz_deq_destroy(&run->deq);
}
