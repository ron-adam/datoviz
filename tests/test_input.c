#include "../include/datoviz/input.h"
#include "proto.h"
#include "tests.h"



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
/*  Mouse tests                                                                                  */
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

int test_input_mouse_raw(TestContext* tc)
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

    // Destroy the resources.
    dvz_input_destroy(&input);
    _glfw_destroy(w);
    return 0;
}



static void _on_mouse_drag_begin(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("BEGIN mouse drag button %d, mods %d", ev.d.button, ev.d.modifiers);
}

static void _on_mouse_drag(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug(
        "mouse drag button %d %.0fx%.0f, mods %d", ev.d.button, ev.d.pos[0], ev.d.pos[1],
        ev.d.modifiers);
}

static void _on_mouse_drag_end(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("END mouse drag");
    ASSERT(user_data != NULL);
    *((bool*)user_data) = true;
}

int test_input_mouse_drag(TestContext* tc)
{
    // Create an input and window.
    DvzInput input = dvz_input();
    GLFWwindow* w = _glfw_window();
    dvz_input_backend(&input, DVZ_BACKEND_GLFW, w);

    bool dragged = false;
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_DRAG_BEGIN, _on_mouse_drag_begin, NULL);
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_DRAG, _on_mouse_drag, NULL);
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_DRAG_END, _on_mouse_drag_end, &dragged);
    // _glfw_event_loop(w);

    dvz_input_event(
        &input, DVZ_INPUT_KEYBOARD_PRESS,
        (DvzInputEvent){.k = {.key_code = DVZ_KEY_LEFT_CONTROL}});
    dvz_input_event(&input, DVZ_INPUT_MOUSE_MOVE, (DvzInputEvent){.m = {.pos = {10, 10}}});
    dvz_input_event(
        &input, DVZ_INPUT_MOUSE_PRESS, (DvzInputEvent){.b = {.button = DVZ_MOUSE_BUTTON_LEFT}});
    dvz_input_event(&input, DVZ_INPUT_MOUSE_MOVE, (DvzInputEvent){.m = {.pos = {50, 50}}});
    dvz_input_event(&input, DVZ_INPUT_MOUSE_MOVE, (DvzInputEvent){.m = {.pos = {100, 100}}});
    dvz_input_event(
        &input, DVZ_INPUT_MOUSE_RELEASE, (DvzInputEvent){.b = {.button = DVZ_MOUSE_BUTTON_LEFT}});
    // NOTE: wait for the background thread to process
    dvz_deq_wait(&input.deq, 0);
    AT(dragged);

    // Destroy the resources.
    dvz_input_destroy(&input);
    _glfw_destroy(w);
    return 0;
}



static void _on_mouse_click(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("click");
    ASSERT(user_data != NULL);
    *((bool*)user_data) = true;
}

static void _on_mouse_double_click(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("double click");
    ASSERT(user_data != NULL);
    *((bool*)user_data) = true;
}

int test_input_mouse_click(TestContext* tc)
{
    // Create an input and window.
    DvzInput input = dvz_input();
    GLFWwindow* w = _glfw_window();
    dvz_input_backend(&input, DVZ_BACKEND_GLFW, w);

    bool click = false, dbl_click = false;

    dvz_input_callback(&input, DVZ_INPUT_MOUSE_CLICK, _on_mouse_click, &click);
    dvz_input_callback(&input, DVZ_INPUT_MOUSE_DOUBLE_CLICK, _on_mouse_double_click, &dbl_click);

    dvz_input_event(&input, DVZ_INPUT_MOUSE_MOVE, (DvzInputEvent){.m = {.pos = {10, 10}}});

    // Simulate a click.
    dvz_input_event(
        &input, DVZ_INPUT_MOUSE_PRESS, (DvzInputEvent){.b = {.button = DVZ_MOUSE_BUTTON_LEFT}});
    dvz_sleep(5);
    dvz_input_event(
        &input, DVZ_INPUT_MOUSE_RELEASE, (DvzInputEvent){.b = {.button = DVZ_MOUSE_BUTTON_LEFT}});

    dvz_deq_wait(&input.deq, 0);
    AT(click);
    AT(!dbl_click);

    // Simulate a double click.
    for (uint32_t i = 0; i < 2; i++)
    {
        dvz_input_event(
            &input, DVZ_INPUT_MOUSE_PRESS,
            (DvzInputEvent){.b = {.button = DVZ_MOUSE_BUTTON_LEFT}});
        dvz_sleep(5);
        dvz_input_event(
            &input, DVZ_INPUT_MOUSE_RELEASE,
            (DvzInputEvent){.b = {.button = DVZ_MOUSE_BUTTON_LEFT}});
        dvz_sleep(5);
    }

    dvz_deq_wait(&input.deq, 0);
    AT(dbl_click);

    // Destroy the resources.
    dvz_input_destroy(&input);
    _glfw_destroy(w);
    return 0;
}



/*************************************************************************************************/
/*  Keyboard tests                                                                               */
/*************************************************************************************************/

static void _on_key_press(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("key press %d, modifiers %d", ev.k.key_code, ev.k.modifiers);
    DvzKeyCode* k = input->keyboard.keys;
    log_debug("%d key(s) pressed: %d %d %d %d", input->keyboard.key_count, k[0], k[1], k[2], k[3]);
    ASSERT(user_data != NULL);
    *((int*)user_data) = ev.k.key_code;
}

static void _on_key_release(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("key release %d", ev.k.key_code);
    ASSERT(user_data != NULL);
    *((int*)user_data) = DVZ_KEY_NONE;
}

int test_input_keyboard(TestContext* tc)
{
    // Create an input and window.
    DvzInput input = dvz_input();
    GLFWwindow* w = _glfw_window();
    dvz_input_backend(&input, DVZ_BACKEND_GLFW, w);

    // Keyboard callbacks.
    DvzKeyCode key = {0};
    dvz_input_callback(&input, DVZ_INPUT_KEYBOARD_PRESS, _on_key_press, &key);
    dvz_input_callback(&input, DVZ_INPUT_KEYBOARD_RELEASE, _on_key_release, &key);

    // Simulate a key stroke.
    dvz_input_event(
        &input, DVZ_INPUT_KEYBOARD_PRESS, (DvzInputEvent){.k = {.key_code = DVZ_KEY_A}});
    dvz_deq_wait(&input.deq, 0);
    dvz_sleep(10);
    AT(key == DVZ_KEY_A);

    dvz_input_event(
        &input, DVZ_INPUT_KEYBOARD_RELEASE, (DvzInputEvent){.k = {.key_code = DVZ_KEY_A}});
    dvz_deq_wait(&input.deq, 0);
    dvz_sleep(10);
    AT(key == DVZ_KEY_NONE);

    // DEBUG
    // _glfw_event_loop(w);

    // Destroy the resources.
    dvz_input_destroy(&input);
    _glfw_destroy(w);
    return 0;
}



/*************************************************************************************************/
/*  Timer tests                                                                                  */
/*************************************************************************************************/

static void _on_timer_tick(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    log_debug("timer tick #%d, period %d, interval %.6f s", ev.t.tick, ev.t.period, ev.t.interval);
    ASSERT(user_data != NULL);
    *((int64_t*)user_data) = ev.t.tick;
}

int test_input_timer(TestContext* tc)
{
    // Create an input and window.
    DvzInput input = dvz_input();
    dvz_input_backend(&input, DVZ_BACKEND_GLFW, NULL);

    // Timer tick callback.
    int64_t tick = 0;
    dvz_input_callback(&input, DVZ_INPUT_TIMER_TICK, _on_timer_tick, &tick);

    // Add a timer.
    dvz_input_event(&input, DVZ_INPUT_TIMER_ADD, (DvzInputEvent){.ta = {.period = 10}});
    dvz_sleep(50);
    AT(tick >= 3);


    // Pause.
    log_debug("pause");
    dvz_input_event(&input, DVZ_INPUT_TIMER_RUNNING, (DvzInputEvent){.tp = {.is_running = false}});
    dvz_deq_wait(&input.deq, 0);
    int64_t tick_checkpoint = tick;
    dvz_sleep(50);
    AT(tick == tick_checkpoint);

    // Resume.
    log_debug("resume");
    dvz_input_event(&input, DVZ_INPUT_TIMER_RUNNING, (DvzInputEvent){.tp = {.is_running = true}});
    dvz_sleep(50);
    dvz_deq_wait(&input.deq, 0);
    AT((uint64_t)tick >= (uint64_t)tick_checkpoint + 3);
    AT(tick <= 9);


    // Update.
    log_debug("change timer period");
    dvz_input_event(
        &input, DVZ_INPUT_TIMER_UPDATE, (DvzInputEvent){.tu = {.period = 5, .after = 50}});
    dvz_deq_wait(&input.deq, 0);
    tick_checkpoint = tick;
    // Do not start before 50 ms.
    dvz_sleep(30);
    AT(tick == tick_checkpoint);

    // After 50 ms, tick every 5 ms.
    dvz_sleep(100);
    AT(tick >= 40);


    // Remove timer.
    dvz_input_event(&input, DVZ_INPUT_TIMER_REMOVE, (DvzInputEvent){.tr = {.timer_id = 0}});
    dvz_deq_wait(&input.deq, 0);
    tick_checkpoint = tick;
    dvz_sleep(50);
    AT(tick == tick_checkpoint);


    // Destroy the resources.
    dvz_input_destroy(&input);
    return 0;
}
