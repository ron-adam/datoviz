#include "../include/datoviz/input.h"
#include <GLFW/glfw3.h>
// #include "backend_glfw.h"



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

// Process the async events in a background thread.
static void* _input_thread(void* user_data)
{
    DvzInput* input = (DvzInput*)user_data;
    ASSERT(input != NULL);
    // Process both mouse and keyboard events in that thread.
    return _deq_loop(&input->deq, DVZ_INPUT_DEQ_MOUSE);
}

static DvzMouseButton _from_glfw_button(int button)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        return DVZ_MOUSE_BUTTON_LEFT;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        return DVZ_MOUSE_BUTTON_RIGHT;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        return DVZ_MOUSE_BUTTON_MIDDLE;
    else
        return DVZ_MOUSE_BUTTON_NONE;
}



/*************************************************************************************************/
/*  Backend-specific code                                                                        */
/*  NOTE: backend_glfw.h is deprecated, should be replaced by the code below                     */
/*************************************************************************************************/

static void _glfw_move_callback(GLFWwindow* window, double xpos, double ypos)
{
    ASSERT(window != NULL);
    DvzInput* input = (DvzInput*)glfwGetWindowUserPointer(window);
    DvzInputEvent ev = {0};
    ev.m.pos[0] = xpos;
    ev.m.pos[1] = ypos;
    ev.m.modifiers = 0; // TODO
    dvz_input_event(input, DVZ_INPUT_MOUSE_MOVE, ev);
}

static void _glfw_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    ASSERT(window != NULL);
    DvzInput* input = (DvzInput*)glfwGetWindowUserPointer(window);
    DvzInputEvent ev = {0};
    ev.b.button = _from_glfw_button(button);
    ev.b.modifiers = 0; // TODO
    DvzInputType evtype = action == GLFW_PRESS ? DVZ_INPUT_MOUSE_PRESS : DVZ_INPUT_MOUSE_RELEASE;
    dvz_input_event(input, evtype, ev);
}



/*************************************************************************************************/
/*  Input                                                                                        */
/*************************************************************************************************/

DvzInput dvz_input()
{
    DvzInput input = {0};

    // Two queues: mouse and keyboard.
    input.deq = dvz_deq(2);

    // A single proc handling both mouse and keyboard events.
    dvz_deq_proc(&input.deq, 0, 2, (uint32_t[]){DVZ_INPUT_DEQ_MOUSE, DVZ_INPUT_DEQ_KEYBOARD});

    return input;
}



void dvz_input_backend(DvzInput* input, DvzBackend backend, void* window)
{
    ASSERT(input != NULL);
    input->backend = backend;
    input->window = window;
    if (window == NULL)
        return;
    ASSERT(window != NULL);

    input->thread = dvz_thread(_input_thread, input);

    switch (backend)
    {
    case DVZ_BACKEND_GLFW:;
        GLFWwindow* w = (GLFWwindow*)window;

        // The canvas pointer will be available to callback functions.
        glfwSetWindowUserPointer(w, input);

        // Register the mouse move callback.
        // TODO: comment?? if commented, see _glfw_frame_callback
        glfwSetCursorPosCallback(w, _glfw_move_callback);

        // Register the mouse button callback.
        glfwSetMouseButtonCallback(w, _glfw_button_callback);



        // // Register the key callback.
        // glfwSetKeyCallback(w, _glfw_key_callback);

        // // Register the mouse wheel callback.
        // glfwSetScrollCallback(w, _glfw_wheel_callback);

        // // Register a function called at every frame, after event polling and state update
        // dvz_event_callback(
        //     canvas, DVZ_EVENT_INTERACT, 0, DVZ_EVENT_MODE_SYNC, _glfw_frame_callback, NULL);

        break;
    default:
        break;
    }
}



static void _deq_callback(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    ASSERT(item != NULL);
    ASSERT(user_data != NULL);

    DvzInputCallbackPayload* payload = (DvzInputCallbackPayload*)user_data;
    DvzInput* input = payload->input;

    DvzInputEvent* ev = (DvzInputEvent*)item;

    payload->callback(input, *ev, payload->user_data);

    return;
}

// Find the Deq queue index by inspecting the input type used for registering the callback.
static uint32_t _deq_from_input_type(DvzInputType type)
{
    uint32_t idx = DVZ_INPUT_DEQ_MOUSE;
    if (type == DVZ_INPUT_KEYBOARD_PRESS || type == DVZ_INPUT_KEYBOARD_RELEASE ||
        type == DVZ_INPUT_KEYBOARD_STROKE)
        idx = DVZ_INPUT_DEQ_KEYBOARD;
    return idx;
}

void dvz_input_callback(
    DvzInput* input, DvzInputType type, DvzInputCallback callback, void* user_data)
{
    ASSERT(input != NULL);
    ASSERT(input->callback_count < DVZ_INPUT_MAX_CALLBACKS);

    DvzInputCallbackPayload* payload = &input->callbacks[input->callback_count++];
    payload->input = input;
    payload->user_data = user_data;
    payload->callback = callback;
    dvz_deq_callback(&input->deq, _deq_from_input_type(type), (int)type, _deq_callback, payload);
}



void dvz_input_event(DvzInput* input, DvzInputType type, DvzInputEvent ev)
{
    ASSERT(input != NULL);
    uint32_t deq_idx = _deq_from_input_type(type);

    DvzInputEvent* pev = calloc(1, sizeof(DvzInputEvent));
    *pev = ev;
    dvz_deq_enqueue(&input->deq, deq_idx, type, pev);
}



void dvz_input_destroy(DvzInput* input)
{
    ASSERT(input != NULL);

    // Enqueue a STOP task to stop the UL and DL threads.
    dvz_deq_enqueue(&input->deq, DVZ_INPUT_NONE, 0, NULL);

    // Join the thread.
    dvz_thread_join(&input->thread);

    // Destroy the Deq.
    dvz_deq_destroy(&input->deq);
}
