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



static int _key_modifiers(int key_code)
{
    int mods = 0;
    if (key_code == DVZ_KEY_LEFT_CONTROL || key_code == DVZ_KEY_RIGHT_CONTROL)
        mods |= DVZ_KEY_MODIFIER_CONTROL;
    if (key_code == DVZ_KEY_LEFT_SHIFT || key_code == DVZ_KEY_RIGHT_SHIFT)
        mods |= DVZ_KEY_MODIFIER_SHIFT;
    if (key_code == DVZ_KEY_LEFT_ALT || key_code == DVZ_KEY_RIGHT_ALT)
        mods |= DVZ_KEY_MODIFIER_ALT;
    if (key_code == DVZ_KEY_LEFT_SUPER || key_code == DVZ_KEY_RIGHT_SUPER)
        mods |= DVZ_KEY_MODIFIER_SUPER;
    return mods;
}



// Return the position of the key pressed if it is pressed, otherwise return -1.
static int _is_key_pressed(DvzInputKeyboard* keyboard, DvzKeyCode key_code)
{
    ASSERT(keyboard != NULL);
    for (uint32_t i = 0; i < keyboard->key_count; i++)
    {
        if (keyboard->keys[i] == key_code)
            return (int)i;
    }
    return -1;
}



static void _remove_key(DvzInputKeyboard* keyboard, DvzKeyCode key_code, uint32_t pos)
{
    ASSERT(keyboard != NULL);
    // ASSERT(pos >= 0);
    ASSERT(pos < DVZ_INPUT_MAX_KEYS);
    ASSERT(keyboard->key_count > 0);
    ASSERT(pos < keyboard->key_count);
    ASSERT(keyboard->keys[pos] == key_code);

    // When an element is removed, need to shift all keys after one position to the left.
    for (uint32_t i = pos; i < (uint32_t)MIN((int)key_code, DVZ_INPUT_MAX_KEYS - 1); i++)
    {
        keyboard->keys[i] = keyboard->keys[i + 1];
    }
    keyboard->key_count--;

    // Reset the unset positions in the array.
    log_debug(
        "reset %d keys after pos %d", DVZ_INPUT_MAX_KEYS - keyboard->key_count,
        keyboard->key_count);
    memset(
        &keyboard->keys[keyboard->key_count], 0,
        (DVZ_INPUT_MAX_KEYS - keyboard->key_count) * sizeof(DvzKeyCode));
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
    ev.m.modifiers = input->mouse.modifiers;
    dvz_input_event(input, DVZ_INPUT_MOUSE_MOVE, ev);
}



static void _glfw_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    ASSERT(window != NULL);
    DvzInput* input = (DvzInput*)glfwGetWindowUserPointer(window);
    DvzInputEvent ev = {0};
    ev.b.button = _from_glfw_button(button);
    ev.b.modifiers = mods;
    DvzInputType evtype = action == GLFW_PRESS ? DVZ_INPUT_MOUSE_PRESS : DVZ_INPUT_MOUSE_RELEASE;
    dvz_input_event(input, evtype, ev);
}



static void _glfw_wheel_callback(GLFWwindow* window, double dx, double dy)
{
    ASSERT(window != NULL);
    DvzInput* input = (DvzInput*)glfwGetWindowUserPointer(window);
    DvzInputEvent ev = {0};

    ev.w.dir[0] = dx;
    ev.w.dir[1] = dy;
    ev.w.modifiers = input->mouse.modifiers;
    dvz_input_event(input, DVZ_INPUT_MOUSE_WHEEL, ev);
}



static void
_input_proc_pre_callback(DvzDeq* deq, uint32_t deq_idx, int type, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    if (item == NULL)
        return;

    DvzInput* input = (DvzInput*)user_data;
    ASSERT(input != NULL);

    // Update the mouse state after every mouse event.
    if (deq_idx == DVZ_INPUT_DEQ_MOUSE)
    {
        dvz_input_mouse_update(input, type, (DvzInputEvent*)item);
    }

    else if (deq_idx == DVZ_INPUT_DEQ_KEYBOARD)
    {
        dvz_input_keyboard_update(input, type, (DvzInputEvent*)item);
    }
}



static void
_input_proc_post_callback(DvzDeq* deq, uint32_t deq_idx, int type, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    if (item == NULL)
        return;

    DvzInput* input = (DvzInput*)user_data;
    ASSERT(input != NULL);

    // Reset wheel event.
    if (input->mouse.cur_state == DVZ_MOUSE_STATE_WHEEL)
    {
        // log_debug("reset wheel state %d", canvas->frame_idx);
        input->mouse.cur_state = DVZ_MOUSE_STATE_INACTIVE;
    }
}



static void _glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ASSERT(window != NULL);
    DvzInput* input = (DvzInput*)glfwGetWindowUserPointer(window);
    DvzInputEvent ev = {0};

    DvzInputType type =
        action == GLFW_RELEASE ? DVZ_INPUT_KEYBOARD_RELEASE : DVZ_INPUT_KEYBOARD_PRESS;

    // NOTE: we use the GLFW key codes here, should actually do a proper mapping between GLFW
    // key codes and Datoviz key codes.
    ev.k.key_code = key;
    ev.k.modifiers = mods;

    dvz_input_event(input, type, ev);
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
void dvz_input_mouse_update(DvzInput* input, DvzInputType type, DvzInputEvent* pev)
{
    ASSERT(input != NULL);

    DvzInputMouse* mouse = &input->mouse;
    ASSERT(mouse != NULL);

    // TODO?: if input capture, do nothing

    // Manually-set keyboard modifiers, if bypassing glfw.
    int mods = input->keyboard.modifiers;
    mouse->modifiers |= mods;

    DvzInputEvent ev = {0};
    ev = *pev;

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
            mouse->modifiers = mods | ev.b.modifiers;
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
            dvz_input_event_first(
                input, DVZ_INPUT_MOUSE_DRAG_END,
                (DvzInputEvent){
                    .d = {
                        .pos = {mouse->cur_pos[0], mouse->cur_pos[1]},
                        .button = mouse->button,
                        .modifiers = mouse->modifiers}});
            mouse->cur_state = DVZ_MOUSE_STATE_INACTIVE;
        }

        // Double click event.
        else if (time - mouse->click_time < DVZ_MOUSE_DOUBLE_CLICK_MAX_DELAY)
        {
            // NOTE: when releasing, current button is NONE so we must use the previously set
            // button in mouse->button.
            log_trace("double click event on button %d", mouse->button);
            mouse->click_time = time;
            dvz_input_event_first(
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
            dvz_input_event_first(
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

        // Mouse move: start drag.
        // NOTE: do not DRAG if we are clicking, with short press time and shift length
        if (mouse->cur_state == DVZ_MOUSE_STATE_INACTIVE &&
            mouse->button != DVZ_MOUSE_BUTTON_NONE &&
            !(time - mouse->press_time < DVZ_MOUSE_CLICK_MAX_DELAY &&
              mouse->shift_length < DVZ_MOUSE_CLICK_MAX_SHIFT))
        {
            dvz_input_event_first(
                input, DVZ_INPUT_MOUSE_DRAG_BEGIN,
                (DvzInputEvent){
                    .d = {
                        .pos = {mouse->cur_pos[0], mouse->cur_pos[1]},
                        .button = mouse->button,
                        .modifiers = mouse->modifiers}});
            mouse->cur_state = DVZ_MOUSE_STATE_DRAG;
            break; // HACK: avoid enqueueing a DRAG event *after* the DRAG_BEGIN event.
        }

        // Mouse move: is dragging.
        if (mouse->cur_state == DVZ_MOUSE_STATE_DRAG)
        {
            dvz_input_event_first(
                input, DVZ_INPUT_MOUSE_DRAG,
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
/*  Keyboard                                                                                     */
/*************************************************************************************************/

DvzInputKeyboard dvz_input_keyboard()
{
    DvzInputKeyboard keyboard = {0};
    dvz_input_keyboard_reset(&keyboard);
    return keyboard;
}



void dvz_input_keyboard_toggle(DvzInputKeyboard* keyboard, bool enable)
{
    ASSERT(keyboard != NULL);
    keyboard->is_active = enable;
}



void dvz_input_keyboard_reset(DvzInputKeyboard* keyboard)
{
    ASSERT(keyboard != NULL);
    memset(keyboard, 0, sizeof(DvzInputKeyboard));
    keyboard->key_count = 0;
    keyboard->modifiers = 0;
    keyboard->press_time = DVZ_NEVER;
    keyboard->is_active = true;
}



void dvz_input_keyboard_update(DvzInput* input, DvzInputType type, DvzInputEvent* pev)
{
    ASSERT(input != NULL);

    DvzInputKeyboard* keyboard = &input->keyboard;
    ASSERT(keyboard != NULL);

    // TODO?: if input capture, do nothing

    DvzInputEvent ev = {0};
    ev = *pev;

    keyboard->prev_state = keyboard->cur_state;

    double time = input->clock.elapsed;
    DvzKeyCode key = ev.k.key_code;
    int is_pressed = 0;

    if (type == DVZ_INPUT_KEYBOARD_PRESS && time - keyboard->press_time > .025)
    {
        // Find out if the key is already pressed.
        is_pressed = _is_key_pressed(keyboard, key);
        // Make sure we don't reach the max number of keys pressed simultaneously.
        // Also, do not add mod keys in the list of keys pressed.
        if (is_pressed < 0 && keyboard->key_count < DVZ_INPUT_MAX_KEYS && !_is_key_modifier(key))
        {
            // Need to register the key in the keyboard state.
            keyboard->keys[keyboard->key_count++] = key;
        }

        // Register the key modifier in the keyboard state.
        if (_is_key_modifier(key))
        {
            keyboard->modifiers |= _key_modifiers(key);
        }

        // Here, we've ensured that the keyboard state has been updated to register the key
        // pressed, except if the maximum number of keys pressed simultaneously has been reached.
        log_trace("key pressed %d mods %d", key, ev.k.modifiers);
        keyboard->modifiers |= ev.k.modifiers;
        keyboard->press_time = time;
        if (keyboard->cur_state == DVZ_KEYBOARD_STATE_INACTIVE)
            keyboard->cur_state = DVZ_KEYBOARD_STATE_ACTIVE;
    }
    else if (type == DVZ_INPUT_KEYBOARD_RELEASE)
    {
        // HACK
        // keyboard->key_count = 0;

        is_pressed = _is_key_pressed(keyboard, key);
        // If the key released was pressed, remove it from the keyboard state.
        // log_debug("is pressed %d", is_pressed);
        if (is_pressed >= 0)
        {
            ASSERT(is_pressed < DVZ_INPUT_MAX_KEYS);
            _remove_key(keyboard, key, (uint32_t)is_pressed);
        }

        // Remove the key modifier in the keyboard state.
        if (_is_key_modifier(key))
        {
            keyboard->modifiers &= ~_key_modifiers(key);
        }

        if (keyboard->cur_state == DVZ_KEYBOARD_STATE_ACTIVE)
            keyboard->cur_state = DVZ_KEYBOARD_STATE_INACTIVE;
    }
}



/*************************************************************************************************/
/*  Timer                                                                                        */
/*************************************************************************************************/

static DvzTimer* _timer_get(DvzInput* input, uint32_t timer_id)
{
    ASSERT(input != NULL);

    // Look for the timer with the passed timer idx, and mark it as destroyed.
    DvzContainerIterator iter = dvz_container_iterator(&input->timers);
    DvzTimer* timer = NULL;
    while (iter.item != NULL)
    {
        timer = (DvzTimer*)iter.item;
        ASSERT(timer != NULL);
        if (timer->timer_id == timer_id)
            return timer;
        dvz_container_iter(&iter);
    }
    return NULL;
}



static void _timer_add(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);
    DvzTimer* timer = (DvzTimer*)dvz_container_alloc(&input->timers);
    ASSERT(timer != NULL);

    timer->input = input;
    timer->timer_id = ev.ta.timer_id;
    timer->max_count = ev.ta.max_count;
    timer->is_running = true;

    timer->after = ev.ta.after;
    timer->period = ev.ta.period;

    dvz_obj_created(&timer->obj);
    log_debug("add timer #%d", timer->timer_id);
}



static void _timer_running(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);

    // Look for the timer with the passed timer idx, and mark it as destroyed.
    DvzTimer* timer = _timer_get(input, ev.tr.timer_id);
    if (timer != NULL)
        timer->is_running = ev.tp.is_running;
}



static void _timer_remove(DvzInput* input, DvzInputEvent ev, void* user_data)
{
    ASSERT(input != NULL);

    // Look for the timer with the passed timer idx, and mark it as destroyed.
    DvzTimer* timer = _timer_get(input, ev.tr.timer_id);
    if (timer != NULL)
    {
        dvz_obj_destroyed(&timer->obj);
        log_debug("remove timer #%d", timer->timer_id);
    }
}



static bool _timer_should_tick(DvzTimer* timer)
{
    ASSERT(timer != NULL);
    ASSERT(timer->input != NULL);

    // Go through all TIMER callbacks
    double cur_time = timer->input->clock.elapsed;
    // When is the next expected time?
    double expected_time = ((timer->tick + 1) * timer->period) / 1000.0;

    // If we reached the expected time, we raise the TIMER event immediately.
    if (cur_time >= expected_time)
        return true;
    else
        return false;
}



static void _timer_tick(DvzTimer* timer)
{
    ASSERT(timer != NULL);
    ASSERT(timer->input != NULL);

    DvzInputEvent ev = {0};
    ev.t.timer_id = timer->timer_id;
    ev.t.period = timer->period;
    ev.t.after = timer->after;
    ev.t.max_count = timer->max_count;

    double cur_time = timer->input->clock.elapsed;
    // At what time was the last TIMER event for this callback?
    double last_time = (timer->tick * timer->period) / 1000.0;

    ev.t.time = cur_time;
    ev.t.tick = timer->tick;
    // NOTE: this is the time since the last *expected* time of the previous TIMER
    // event, not the actual time.
    ev.t.interval = cur_time - last_time;

    // HACK: release the lock before enqueuing a TIMER event, because the lock is currently
    // acquired. Here, we are in a proc wait callback, called while waiting for the queue to be
    // non-empty. The waiting acquires the lock.
    pthread_mutex_unlock(&timer->input->deq.procs[0].lock);
    dvz_input_event(timer->input, DVZ_INPUT_TIMER_TICK, ev);
    pthread_mutex_lock(&timer->input->deq.procs[0].lock);

    timer->tick++;
}



static void _timer_ticks(DvzDeq* deq, void* user_data)
{
    ASSERT(deq != NULL);
    DvzInput* input = (DvzInput*)user_data;
    ASSERT(input != NULL);

    // Update the clock struct every 1 ms (proc wait callback).
    _clock_set(&input->clock);

    DvzContainerIterator iter = dvz_container_iterator(&input->timers);
    DvzTimer* timer = NULL;
    while (iter.item != NULL)
    {
        timer = (DvzTimer*)iter.item;
        ASSERT(timer != NULL);
        if (dvz_obj_is_created(&timer->obj) && timer->is_running)
        {
            if (_timer_should_tick(timer))
            {
                _timer_tick(timer);
            }
        }
        dvz_container_iter(&iter);
    }
}



/*************************************************************************************************/
/*  Input                                                                                        */
/*************************************************************************************************/

DvzInput dvz_input()
{
    DvzInput input = {0};

    input.mouse = dvz_input_mouse();
    input.keyboard = dvz_input_keyboard();
    input.timers =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzTimer), DVZ_OBJECT_TYPE_TIMER);

    _clock_init(&input.clock);

    // Queues: mouse, keyboard, timer.
    input.deq = dvz_deq(3);

    // A single proc handling both mouse and keyboard events.
    dvz_deq_proc(
        &input.deq, 0, 3,
        (uint32_t[]){DVZ_INPUT_DEQ_MOUSE, DVZ_INPUT_DEQ_KEYBOARD, DVZ_INPUT_DEQ_TIMER});

    return input;
}



void dvz_input_backend(DvzInput* input, DvzBackend backend, void* window)
{
    ASSERT(input != NULL);
    input->backend = backend;
    input->window = window;
    // if (window == NULL)
    //     return;
    // ASSERT(window != NULL);

    input->thread = dvz_thread(_input_thread, input);

    switch (backend)
    {
    case DVZ_BACKEND_GLFW:;
        GLFWwindow* w = (GLFWwindow*)window;

        // Register a proc callback to update the mouse and keyboard state after every event.
        dvz_deq_proc_callback(
            &input->deq, 0, DVZ_DEQ_PROC_CALLBACK_PRE, _input_proc_pre_callback, input);

        // Register a proc callback to update the mouse and keyboard state after each dequeue.
        dvz_deq_proc_callback(
            &input->deq, 0, DVZ_DEQ_PROC_CALLBACK_POST, _input_proc_post_callback, input);

        // The canvas pointer will be available to callback functions.
        if (w)
        {
            glfwSetWindowUserPointer(w, input);


            // Mouse callbacks.

            // Register the mouse move callback.
            // TODO: comment?? if commented, see _glfw_frame_callback
            glfwSetCursorPosCallback(w, _glfw_move_callback);

            // Register the mouse button callback.
            glfwSetMouseButtonCallback(w, _glfw_button_callback);

            // Register the mouse wheel callback.
            glfwSetScrollCallback(w, _glfw_wheel_callback);


            // Keyboard callbacks.

            // Register the key callback.
            glfwSetKeyCallback(w, _glfw_key_callback);
        }


        // Timer callbacks.
        dvz_input_callback(input, DVZ_INPUT_TIMER_ADD, _timer_add, NULL);
        dvz_input_callback(input, DVZ_INPUT_TIMER_RUNNING, _timer_running, NULL);
        dvz_input_callback(input, DVZ_INPUT_TIMER_REMOVE, _timer_remove, NULL);

        // In the input thread, while waiting for input events, every millisecond, check if there
        // are active timers, and fire TIMER_TICK events if needed.
        dvz_deq_proc_wait_delay(&input->deq, 0, 1);
        dvz_deq_proc_wait_callback(&input->deq, 0, _timer_ticks, input);



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



static void _input_event(DvzInput* input, DvzInputType type, DvzInputEvent ev, bool enqueue_first)
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
    if (!enqueue_first)
        dvz_deq_enqueue(&input->deq, deq_idx, type, pev);
    else
        dvz_deq_enqueue_first(&input->deq, deq_idx, type, pev);
}

void dvz_input_event(DvzInput* input, DvzInputType type, DvzInputEvent ev)
{
    _input_event(input, type, ev, false);
}

void dvz_input_event_first(DvzInput* input, DvzInputType type, DvzInputEvent ev)
{
    _input_event(input, type, ev, true);
}



void dvz_input_destroy(DvzInput* input)
{
    ASSERT(input != NULL);

    // Enqueue a STOP task to stop the UL and DL threads.
    dvz_deq_enqueue(&input->deq, DVZ_INPUT_NONE, 0, NULL);

    // Join the thread.
    dvz_thread_join(&input->thread);

    // Destroy the timers.
    DvzContainerIterator iter = dvz_container_iterator(&input->timers);
    DvzTimer* timer = NULL;
    while (iter.item != NULL)
    {
        timer = (DvzTimer*)iter.item;
        ASSERT(timer != NULL);
        dvz_obj_destroyed(&timer->obj);
        dvz_container_iter(&iter);
    }
    dvz_container_destroy(&input->timers);

    // Destroy the Deq.
    dvz_deq_destroy(&input->deq);
}
