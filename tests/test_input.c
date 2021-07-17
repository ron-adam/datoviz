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

int test_input_1(TestContext* tc)
{
    DvzInput input = dvz_input();
    GLFWwindow* w = _glfw_window();
    dvz_input_backend(&input, DVZ_BACKEND_GLFW, w);

    vec2 pos = {0};
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_MOVE, _on_mouse_move, &pos);

    // Force the mouse position at the center of the window, poll the events, and ensure the mouse
    // move callback has been properly called in the background thread.
    _glfw_set_mouse_pos(w, (vec2){WIDTH / 2, HEIGHT / 2});

    // Check that the on_mouse_move callback modified the pos vec2.
    AT(pos[0] == WIDTH / 2);
    AT(pos[1] == HEIGHT / 2);

    // Destroy the input instance.
    dvz_input_destroy(&input);

    // Destroy the window.
    _glfw_destroy(w);
    return 0;
}
