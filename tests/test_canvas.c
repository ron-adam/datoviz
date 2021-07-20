#include "../external/video.h"
#include "../include/datoviz/canvas.h"
#include "../include/datoviz/controls.h"
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

static TestVisual triangle(DvzCanvas* canvas, const char* suffix)
{
    ASSERT(canvas != NULL);
    DvzGpu* gpu = canvas->gpu;
    ASSERT(gpu != NULL);

    TestVisual visual = {0};
    visual.gpu = gpu;
    visual.renderpass = &canvas->render.renderpass;
    visual.framebuffers = &canvas->render.framebuffers;

    // Make the graphics.
    visual.graphics = triangle_graphics(gpu, visual.renderpass, suffix);

    return visual;
}

static void triangle_refill(DvzCanvas* canvas, TestVisual* visual, uint32_t idx)
{
    ASSERT(canvas != NULL);
    ASSERT(visual != NULL);

    // Take the first command buffers, which corresponds to the default canvas render command//
    // buffer.
    // ASSERT(ev.u.rf.cmd_count == 1);
    DvzCommands* cmds = &canvas->cmds_render;
    ASSERT(cmds->queue_idx == DVZ_DEFAULT_QUEUE_RENDER);

    triangle_commands(
        cmds, idx, &canvas->render.renderpass, &canvas->render.framebuffers, //
        &visual->graphics, &visual->bindings, visual->br);
}

static void triangle_upload(DvzCanvas* canvas, TestVisual* visual)
{
    ASSERT(canvas != NULL);
    ASSERT(visual != NULL);
    DvzGpu* gpu = visual->gpu;
    ASSERT(gpu != NULL);

    // Create the buffer.
    VkDeviceSize size = 3 * sizeof(TestVertex);
    visual->br = dvz_ctx_buffers(gpu->context, DVZ_BUFFER_TYPE_VERTEX, 1, size);
    TestVertex data[3] = TRIANGLE_VERTICES;
    visual->data = calloc(size, 1);
    memcpy(visual->data, data, size);
    dvz_upload_buffer(gpu->context, visual->br, 0, size, data);
}



static void _offscreen_render(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);

    uint32_t f = canvas->cur_frame;
    uint32_t img_idx = canvas->render.swapchain.img_idx;

    DvzSubmit* s = &canvas->render.submit;

    // Reset the Submit instance before adding the command buffers.
    dvz_submit_reset(s);

    // Add the command buffers to the submit instance.
    if (canvas->cmds_render.obj.status == DVZ_OBJECT_STATUS_CREATED)
        dvz_submit_commands(s, &canvas->cmds_render);

    // Send the Submit instance.
    dvz_submit_send(s, img_idx, &canvas->sync.fences_render_finished, f);

    // Wait for the render to finish.
    dvz_fences_wait(&canvas->sync.fences_render_finished, f);
}



/*************************************************************************************************/
/*  Blank canvas                                                                                 */
/*************************************************************************************************/

static void _on_mouse_move(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    log_debug("mouse position: %.0fx%.0f", ev.m.pos[0], ev.m.pos[1]);
}

int test_canvas_blank(TestContext* tc)
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



int test_canvas_offscreen(TestContext* tc)
{
    DvzApp* app = tc->app;
    DvzGpu* gpu = dvz_gpu_best(app);

    DvzCanvas* canvas = dvz_canvas(gpu, WIDTH, HEIGHT, DVZ_CANVAS_FLAGS_OFFSCREEN);
    ASSERT(canvas->app != NULL);
    dvz_canvas_create(canvas);

    // Create command buffers and render them.
    DvzCommands* cmds = &canvas->cmds_render;
    for (uint32_t cmd_idx = 0; cmd_idx < canvas->render.swapchain.img_count; cmd_idx++)
    {
        dvz_cmd_reset(cmds, cmd_idx);
        dvz_cmd_begin(cmds, cmd_idx);
        dvz_cmd_begin_renderpass(
            cmds, cmd_idx, &canvas->render.renderpass, &canvas->render.framebuffers);
        dvz_cmd_end_renderpass(cmds, cmd_idx);
        dvz_cmd_end(cmds, cmd_idx);
    }
    _offscreen_render(canvas);

    dvz_canvas_destroy(canvas);
    return 0;
}
