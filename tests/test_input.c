#include "../include/datoviz/input.h"
#include "proto.h"
#include "tests.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

static GLFWwindow* _glfw_window()
{
    if (!glfwInit())
        exit(1);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* w = glfwCreateWindow(WIDTH, HEIGHT, "Input test window", NULL, NULL);
    ASSERT(w != NULL);
    return w;
}

static void _glfw_set_mouse_pos(GLFWwindow* w, vec2 pos)
{
    for (uint32_t i = 0; i < 10; i++)
    {
        glfwSetCursorPos(w, (double)pos[0], (double)pos[1]);
        glfwPollEvents();
    }
}

static void _glfw_event_loop(GLFWwindow* w)
{
    ASSERT(w != NULL);
    while (!glfwWindowShouldClose(w))
    {
        glfwPollEvents();
    }
}

static void _glfw_destroy(GLFWwindow* w)
{
    ASSERT(w != NULL);
    glfwDestroyWindow(w);
    glfwTerminate();
}



/*************************************************************************************************/
/*  Input tests                                                                                  */
/*************************************************************************************************/

static void _on_mouse_move(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("mouse position: %.0fx%.0f", ev.m.pos[0], ev.m.pos[1]);

    ASSERT(user_data != NULL);
    vec2* pos = (vec2*)user_data;
    pos[0][0] = ev.m.pos[0];
    pos[0][1] = ev.m.pos[1];
}

static void _on_mouse_button(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("mouse button: %d", ev.b.button);

    ASSERT(user_data != NULL);
    int* button = (int*)user_data;
    *button = ev.b.button;
}

static void _on_mouse_wheel(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("mouse wheel: %.1fx%.1f", ev.w.dir[0], ev.w.dir[1]);

    ASSERT(user_data != NULL);
    vec2* dir = (vec2*)user_data;
    dir[0][0] = ev.w.dir[0];
    dir[0][1] = ev.w.dir[1];
}

int test_input_mouse(TestContext* tc)
{
    // Create an input and window.
    DvzInput input = dvz_input();
    GLFWwindow* w = _glfw_window();
    dvz_input_backend(&input, DVZ_BACKEND_GLFW, w);


    // Mouse position.
    vec2 pos = {0};
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_MOVE, _on_mouse_move, &pos);

    // Force the mouse position at the center of the window, poll the events, and ensure the mouse
    // move callback has been properly called in the background thread.
    _glfw_set_mouse_pos(w, (vec2){WIDTH / 2, HEIGHT / 2});

    // Check that the on_mouse_move callback modified the pos vec2.
    AT(pos[0] != 0);
    AT(pos[1] != 0);


    // Mouse button press.
    int button = 0;
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_PRESS, _on_mouse_button, &button);
    dvz_input_event(
        &input, DVZ_INPUT_MOUSE_PRESS, (DvzInputEvent){.b.button = DVZ_MOUSE_BUTTON_LEFT});
    // HACK: wait for the background thread to process the mouse press callback and modify the
    // button variable.
    dvz_sleep(10);
    // _glfw_event_loop(w);
    AT(button == (int)DVZ_MOUSE_BUTTON_LEFT);


    // Mouse button release.
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_RELEASE, _on_mouse_button, &button);
    dvz_input_event(
        &input, DVZ_INPUT_MOUSE_RELEASE, (DvzInputEvent){.b.button = DVZ_MOUSE_BUTTON_RIGHT});
    // HACK: wait for the background thread to process the mouse press callback and modify the
    // button variable.
    dvz_sleep(10);
    // _glfw_event_loop(w);
    AT(button == (int)DVZ_MOUSE_BUTTON_RIGHT);


    // Mouse wheel.
    vec2 dir = {0};
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_WHEEL, _on_mouse_wheel, dir);
    dvz_input_event(&input, DVZ_INPUT_MOUSE_WHEEL, (DvzInputEvent){.w.dir = {0, 1}});
    // HACK: wait for the background thread to process the mouse press callback and modify the
    // button variable.
    dvz_sleep(10);
    // _glfw_event_loop(w);
    AT(dir[1] == 1);


    // _glfw_event_loop(w);


    // Destroy the resources.
    dvz_input_destroy(&input);
    _glfw_destroy(w);
    return 0;
}
