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

static void _change_clear_color(DvzCanvas* canvas, vec3 rgb)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);
    ASSERT(canvas->app->run != NULL);

    DvzCanvasEventClearColor* ev = calloc(1, sizeof(DvzCanvasEventClearColor));
    ev->canvas = canvas;
    ev->r = rgb[0];
    ev->g = rgb[1];
    ev->b = rgb[2];
    dvz_deq_enqueue(&canvas->app->run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_CLEAR_COLOR, ev);
}



/*************************************************************************************************/
/*  Run tests                                                                                    */
/*************************************************************************************************/

int test_run_1(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    // Create a canvas.
    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, 0);
    // dvz_canvas_vsync(canvas, false);
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

    // Change the canvas clear color.
    _change_clear_color(canvas, (vec3){1, 0, 0});

    // Event loop.
    dvz_run_loop(run, 10);

    // Add a canvas.
    {
        DvzCanvasEventNew* ev = calloc(1, sizeof(DvzCanvasEventNew));
        ev->gpu = gpu;
        ev->width = WIDTH;
        ev->height = HEIGHT;
        dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_NEW, ev);
    }

    // Event loop.
    dvz_run_loop(run, 10);

    // Delete a canvas.
    {
        DvzCanvasEvent* ev = calloc(1, sizeof(DvzCanvasEvent));
        ev->canvas = canvas;
        dvz_deq_enqueue(&run->deq, DVZ_RUN_DEQ_MAIN, DVZ_RUN_CANVAS_DELETE, ev);
    }

    // Event loop.
    dvz_run_loop(run, 10);

    dvz_run_destroy(run);
    return 0;
}



static void _on_mouse_move(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    DvzCanvas* canvas = (DvzCanvas*)user_data;
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);
    ASSERT(canvas->app->run != NULL);
    // if (canvas->deleting)
    //     return;
    log_debug("mouse position: %.0fx%.0f", ev.m.pos[0], ev.m.pos[1]);
    _change_clear_color(canvas, (vec3){1, ev.m.pos[0] / WIDTH, ev.m.pos[1] / HEIGHT});
}

int test_run_3(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    // Create a canvas.
    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, 0);
    dvz_canvas_create(canvas);

    // Mouse move callback.
    dvz_input_callback(&canvas->input, DVZ_INPUT_MOUSE_MOVE, _on_mouse_move, canvas);

    // Create a run instance.
    DvzRun* run = dvz_run(app);

    // Event loop.
    dvz_run_loop(run, 10);

    dvz_run_destroy(run);
    return 0;
}
