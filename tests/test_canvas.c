// #include "../external/video.h"
#include "../include/datoviz/canvas.h"
#include "../include/datoviz/controls.h"
#include "../src/run_utils.h"
#include "../src/transfer_utils.h"
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
/*  Blank canvas                                                                                 */
/*************************************************************************************************/

static void _on_mouse_move(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    log_debug("mouse position: %.0fx%.0f", ev.m.pos[0], ev.m.pos[1]);
}

int test_canvas_window(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, 0);
    ASSERT(canvas->app != NULL);

    dvz_canvas_create(canvas);

    uvec2 size = {0};

    // Framebuffer size.
    dvz_canvas_size(canvas, DVZ_CANVAS_SIZE_FRAMEBUFFER, size);
    log_debug("canvas framebuffer size is %dx%d", size[0], size[1]);
    ASSERT(size[0] > 0);
    ASSERT(size[1] > 0);

    // Screen size.
    dvz_canvas_size(canvas, DVZ_CANVAS_SIZE_SCREEN, size);
    log_debug("canvas screen size is %dx%d", size[0], size[1]);
    ASSERT(size[0] > 0);
    ASSERT(size[1] > 0);

    // Mouse position.
    dvz_input_callback(&canvas->input, DVZ_INPUT_MOUSE_MOVE, _on_mouse_move, NULL);


    dvz_canvas_destroy(canvas);
    return 0;
}



int test_canvas_blank(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, DVZ_CANVAS_FLAGS_OFFSCREEN);
    ASSERT(canvas->app != NULL);
    dvz_canvas_create(canvas);

    // Create command buffers and render them.
    _canvas_render(canvas);

    // Check blank canvas.
    uint8_t* rgba = dvz_screenshot(canvas, true);
    uint8_t exp[4] = {18, 8, 0, 255};
    if (rgba != NULL)
    {
        for (uint32_t i = 0; i < WIDTH * HEIGHT * 4 * sizeof(uint8_t); i++)
            AT(rgba[i] == exp[i % 4]);
        FREE(rgba);
    }

    dvz_canvas_destroy(canvas);
    return 0;
}



int test_canvas_triangle(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, DVZ_CANVAS_FLAGS_OFFSCREEN);
    ASSERT(canvas->app != NULL);
    dvz_canvas_create(canvas);

    TestVisual visual = triangle(canvas, "");

    // Bindings and graphics pipeline.
    visual.bindings = dvz_bindings(&visual.graphics.slots, 1);
    dvz_bindings_update(&visual.bindings);
    dvz_graphics_create(&visual.graphics);

    // Triangle data.
    triangle_upload(canvas, &visual);

    // Fille the triangle command buffer.
    triangle_refill(canvas, &visual, 0);

    // Create command buffers and render them.
    _canvas_render(canvas);
    dvz_gpu_wait(gpu);

    int res = check_canvas(canvas, "test_canvas_triangle");

    TestVertex data2[3] = {0};
    dvz_download_buffer(gpu->context, visual.br, 0, sizeof(data2), data2);
    AT(memcmp(data2, visual.data, sizeof(data2)) == 0);

    destroy_visual(&visual);
    dvz_canvas_destroy(canvas);
    return res;
}
