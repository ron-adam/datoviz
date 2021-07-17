#include "../include/datoviz/input.h"
#include <GLFW/glfw3.h>
// #include "backend_glfw.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_NEVER                        -1000000
#define DVZ_MOUSE_CLICK_MAX_DELAY        .25
#define DVZ_MOUSE_CLICK_MAX_SHIFT        5
#define DVZ_MOUSE_DOUBLE_CLICK_MAX_DELAY .2
#define DVZ_KEY_PRESS_DELAY              .05



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



static bool _is_key_modifier(DvzKeyCode key)
{
    return (
        key == DVZ_KEY_LEFT_SHIFT || key == DVZ_KEY_RIGHT_SHIFT || key == DVZ_KEY_LEFT_CONTROL ||
        key == DVZ_KEY_RIGHT_CONTROL || key == DVZ_KEY_LEFT_ALT || key == DVZ_KEY_RIGHT_ALT ||
        key == DVZ_KEY_LEFT_SUPER || key == DVZ_KEY_RIGHT_SUPER);
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

static void _glfw_wheel_callback(GLFWwindow* window, double dx, double dy)
{
    ASSERT(window != NULL);
    DvzInput* input = (DvzInput*)glfwGetWindowUserPointer(window);
    DvzInputEvent ev = {0};

    // HACK: glfw doesn't seem to give a way to probe the keyboard modifiers while using the mouse
    // wheel, so we have to determine the modifiers manually.
    // Limitation: a single modifier is allowed here.
    // TODO: allow for multiple simultlaneous modifiers, will require updating the keyboard struct
    // so that it supports multiple simultaneous keys

    ev.w.dir[0] = dx;
    ev.w.dir[1] = dy;
    // ev.w.modifiers // TODO
    dvz_input_event(input, DVZ_INPUT_MOUSE_WHEEL, ev);
}



/*************************************************************************************************/
/*  Mouse                                                                                        */
/*************************************************************************************************/

DvzInputMouse dvz_input_mouse()
{
    DvzInputMouse mouse = {0};
    dvz_input_mouse_reset(&mouse);
    return mouse;
}



void dvz_input_mouse_toggle(DvzInputMouse* mouse, bool enable)
{
    ASSERT(mouse != NULL);
    mouse->is_active = enable;
}



void dvz_input_mouse_reset(DvzInputMouse* mouse)
{
    ASSERT(mouse != NULL);
    memset(mouse, 0, sizeof(DvzInputMouse));
    mouse->button = DVZ_MOUSE_BUTTON_NONE;
    glm_vec2_zero(mouse->cur_pos);
    glm_vec2_zero(mouse->press_pos);
    glm_vec2_zero(mouse->last_pos);
    mouse->cur_state = DVZ_MOUSE_STATE_INACTIVE;
    mouse->press_time = DVZ_NEVER;
    mouse->click_time = DVZ_NEVER;
    mouse->is_active = true;
}



// Called after every mouse callback.
void dvz_input_mouse_update(DvzInput* input, DvzInputType type, DvzInputEvent ev)
{
    ASSERT(input != NULL);

    DvzInputMouse* mouse = &input->mouse;
    ASSERT(mouse != NULL);
    // TODO?: if input capture, do nothing

    // log_debug("mouse event %d", canvas->frame_idx);
    mouse->prev_state = mouse->cur_state;

    double time = input->clock.elapsed;

    // Update the last pos.
    glm_vec2_copy(mouse->cur_pos, mouse->last_pos);

    // Reset click events as soon as the next loop iteration after they were raised.
    if (mouse->cur_state == DVZ_MOUSE_STATE_CLICK ||
        mouse->cur_state == DVZ_MOUSE_STATE_DOUBLE_CLICK)
    {
        mouse->cur_state = DVZ_MOUSE_STATE_INACTIVE;
        mouse->button = DVZ_MOUSE_BUTTON_NONE;
    }

    // Net distance in pixels since the last press event.
    vec2 shift = {0};

    switch (type)
    {

    case DVZ_INPUT_MOUSE_PRESS:

        // Press event.
        if (mouse->press_time == DVZ_NEVER)
        {
            glm_vec2_copy(mouse->cur_pos, mouse->press_pos);
            mouse->press_time = time;
            mouse->button = ev.b.button;
            // Keep track of the modifiers used for the press event.
            mouse->modifiers = ev.b.modifiers;
        }
        mouse->shift_length = 0;
        break;

    case DVZ_INPUT_MOUSE_RELEASE:
        // Release event.

        // End drag.
        if (mouse->cur_state == DVZ_MOUSE_STATE_DRAG)
        {
            log_trace("end drag event");
            mouse->button = DVZ_MOUSE_BUTTON_NONE;
            mouse->modifiers = 0; // Reset the mouse key modifiers

            // dvz_event_mouse_drag_end(canvas, mouse->cur_pos, mouse->button, mouse->modifiers);
            dvz_input_event(
                input, DVZ_INPUT_MOUSE_DRAG_END,
                (DvzInputEvent){
                    .d = {
                        .pos = {mouse->cur_pos[0], mouse->cur_pos[1]},
                        .button = mouse->button,
                        .modifiers = mouse->modifiers}});
        }

        // Double click event.
        else if (time - mouse->click_time < DVZ_MOUSE_DOUBLE_CLICK_MAX_DELAY)
        {
            // NOTE: when releasing, current button is NONE so we must use the previously set
            // button in mouse->button.
            log_trace("double click event on button %d", mouse->button);
            mouse->click_time = time;
            // dvz_event_mouse_double_click(canvas, mouse->cur_pos, mouse->button,
            // mouse->modifiers);
            dvz_input_event(
                input, DVZ_INPUT_MOUSE_DOUBLE_CLICK,
                (DvzInputEvent){
                    .c = {
                        .pos = {mouse->cur_pos[0], mouse->cur_pos[1]},
                        .button = mouse->button,
                        .modifiers = mouse->modifiers}});
        }

        // Click event.
        else if (
            time - mouse->press_time < DVZ_MOUSE_CLICK_MAX_DELAY &&
            mouse->shift_length < DVZ_MOUSE_CLICK_MAX_SHIFT)
        {
            log_trace("click event on button %d", mouse->button);
            mouse->cur_state = DVZ_MOUSE_STATE_CLICK;
            mouse->click_time = time;
            // dvz_event_mouse_click(canvas, mouse->cur_pos, mouse->button, mouse->modifiers);
            dvz_input_event(
                input, DVZ_INPUT_MOUSE_CLICK,
                (DvzInputEvent){
                    .c = {
                        .pos = {mouse->cur_pos[0], mouse->cur_pos[1]},
                        .button = mouse->button,
                        .modifiers = mouse->modifiers}});
        }

        else
        {
            // Reset the mouse button state.
            mouse->button = DVZ_MOUSE_BUTTON_NONE;
        }

        mouse->press_time = DVZ_NEVER;
        mouse->shift_length = 0;
        break;


    case DVZ_INPUT_MOUSE_MOVE:
        glm_vec2_copy(ev.m.pos, mouse->cur_pos);

        // Update the distance since the last press position.
        if (mouse->button != DVZ_MOUSE_BUTTON_NONE)
        {
            glm_vec2_sub(mouse->cur_pos, mouse->press_pos, shift);
            mouse->shift_length = glm_vec2_norm(shift);
        }

        // Mouse move.
        // NOTE: do not DRAG if we are clicking, with short press time and shift length
        if (mouse->cur_state == DVZ_MOUSE_STATE_INACTIVE &&
            mouse->button != DVZ_MOUSE_BUTTON_NONE &&
            !(time - mouse->press_time < DVZ_MOUSE_CLICK_MAX_DELAY &&
              mouse->shift_length < DVZ_MOUSE_CLICK_MAX_SHIFT))
        {
            log_trace("drag event on button %d", mouse->button);
            // dvz_event_mouse_drag(canvas, mouse->cur_pos, mouse->button, mouse->modifiers);
            dvz_input_event(
                input, DVZ_INPUT_MOUSE_DRAG_BEGIN,
                (DvzInputEvent){
                    .d = {
                        .pos = {mouse->cur_pos[0], mouse->cur_pos[1]},
                        .button = mouse->button,
                        .modifiers = mouse->modifiers}});
        }
        // log_trace("mouse mouse %.1fx%.1f", mouse->cur_pos[0], mouse->cur_pos[1]);
        break;


    case DVZ_INPUT_MOUSE_WHEEL:
        glm_vec2_copy(ev.w.pos, mouse->cur_pos);
        glm_vec2_copy(ev.w.dir, mouse->wheel_delta);
        mouse->cur_state = DVZ_MOUSE_STATE_WHEEL;
        mouse->modifiers = ev.w.modifiers;
        break;

    default:
        break;
    }
}



/*************************************************************************************************/
/*  Input                                                                                        */
/*************************************************************************************************/

DvzInput dvz_input()
{
    DvzInput input = {0};
    input.mouse = dvz_input_mouse();
    // input.keyboard = dvz_input_keyboard();

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

        // Register the mouse wheel callback.
        glfwSetScrollCallback(w, _glfw_wheel_callback);



        // // Register the key callback.
        // glfwSetKeyCallback(w, _glfw_key_callback);

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

    // NOTE: prevent input enqueueing for mouse and keyboard if this input is inactive.
    if (deq_idx == DVZ_INPUT_DEQ_MOUSE && !input->mouse.is_active)
        return;
    if (deq_idx == DVZ_INPUT_DEQ_KEYBOARD && !input->keyboard.is_active)
        return;

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
