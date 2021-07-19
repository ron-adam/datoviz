#include "../include/datoviz/canvas.h"
#include "../include/datoviz/run.h"
#include "proto.h"
#include "tests.h"



/*************************************************************************************************/
/*  Macros                                                                                       */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Run tests                                                                                    */
/*************************************************************************************************/

int test_run_1(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    // Create a canvas.
    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, 0);
    dvz_canvas_create(canvas);

    // Create a run instance.
    DvzRun* run = dvz_run(app);

    // Event loop.
    dvz_run_loop(run, 10);

    // Framebuffer size.
    uvec2 size = {0};
    dvz_canvas_size(canvas, DVZ_CANVAS_SIZE_FRAMEBUFFER, size);

    // Check blank canvas.
    uint8_t* rgb = dvz_screenshot(canvas, false);
    if (rgb != NULL)
    {
        for (uint32_t i = 0; i < size[0] * size[1] * 3 * sizeof(uint8_t); i++)
        {
            AT(rgb[i] == (i % 3 == 0 ? 0 : (i % 3 == 1 ? 8 : 18)))
        }
        FREE(rgb);
    }

    dvz_run_destroy(run);
    return 0;
}



int test_run_2(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    // Create a canvas.
    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, 0);
    dvz_canvas_create(canvas);

    // Create a run instance.
    DvzRun* run = dvz_run(app);

    // Event loop.
    dvz_run_loop(run, 10);

    // Add a canvas.
    {
        DvzRunCanvasNewEvent* ev = calloc(1, sizeof(DvzRunCanvasNewEvent));
        ev->gpu = gpu;
        ev->width = WIDTH;
        ev->height = HEIGHT;
        dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_NEW, ev);
    }

    // Event loop.
    dvz_run_loop(run, 10);

    // Delete a canvas.
    {
        DvzRunCanvasDefaultEvent* ev = calloc(1, sizeof(DvzRunCanvasDefaultEvent));
        ev->canvas = canvas;
        dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_DELETE, ev);
    }

    // Event loop.
    dvz_run_loop(run, 10);

    dvz_run_destroy(run);
    return 0;
}
