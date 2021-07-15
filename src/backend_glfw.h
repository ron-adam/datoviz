#ifndef DVZ_CANVAS_BACKEND_GLFW_HEADER
#define DVZ_CANVAS_BACKEND_GLFW_HEADER

#include "../include/datoviz/canvas.h"
#include "../include/datoviz/input.h"
#include "../include/datoviz/keycode.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Backend-specific event callbacks                                                             */
/*************************************************************************************************/

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

static void _glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    DvzCanvas* canvas = (DvzCanvas*)glfwGetWindowUserPointer(window);
    ASSERT(canvas != NULL);
    ASSERT(canvas->window != NULL);
    if (!canvas->keyboard.is_active)
        return;

    // Special handling of ESC key.
    if (canvas->window->close_on_esc && action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
    {
        canvas->window->obj.status = DVZ_OBJECT_STATUS_NEED_DESTROY;
        return;
    }

    DvzKeyCode key_code = {0};

    // NOTE: we use the GLFW key codes here, should actually do a proper mapping between GLFW
    // key codes and Datoviz key codes.
    key_code = key;

    // Find the key event type.
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        dvz_event_key_press(canvas, key_code, mods);
    else
        dvz_event_key_release(canvas, key_code, mods);
}

static void _glfw_wheel_callback(GLFWwindow* window, double dx, double dy)
{
    DvzCanvas* canvas = (DvzCanvas*)glfwGetWindowUserPointer(window);
    ASSERT(canvas != NULL);
    ASSERT(canvas->window != NULL);
    if (!canvas->mouse.is_active)
        return;

    // HACK: glfw doesn't seem to give a way to probe the keyboard modifiers while using the mouse
    // wheel, so we have to determine the modifiers manually.
    // Limitation: a single modifier is allowed here.
    // TODO: allow for multiple simultlaneous modifiers, will require updating the keyboard struct
    // so that it supports multiple simultaneous keys
    dvz_event_mouse_wheel(
        canvas, canvas->mouse.cur_pos, (vec2){dx, dy}, _key_modifiers(canvas->keyboard.key_code));
}

static void _glfw_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    DvzCanvas* canvas = (DvzCanvas*)glfwGetWindowUserPointer(window);
    ASSERT(canvas != NULL);
    ASSERT(canvas->window != NULL);
    if (!canvas->mouse.is_active)
        return;

    // Map mouse button.
    DvzMouseButton b = {0};
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        b = DVZ_MOUSE_BUTTON_LEFT;
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        b = DVZ_MOUSE_BUTTON_RIGHT;
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        b = DVZ_MOUSE_BUTTON_MIDDLE;

    // Find mouse button action type
    // NOTE: Datoviz modifiers code must match GLFW
    if (action == GLFW_PRESS)
        dvz_event_mouse_press(canvas, b, mods);
    else
        dvz_event_mouse_release(canvas, b, mods);
}

static void _enqueue_mouse_move(DvzCanvas* canvas, vec2 pos, int modifiers)
{
    DvzMouseMoveEvent* ev = calloc(1, sizeof(DvzMouseMoveEvent));
    ev->pos[0] = pos[0];
    ev->pos[1] = pos[1];
    ev->modifiers = modifiers;
    dvz_deq_enqueue(&canvas->deq, DVZ_CANVAS_DEQ_MOUSE, DVZ_CANVAS_MOUSE_MOVE, ev);
}

static void _glfw_move_callback(GLFWwindow* window, double xpos, double ypos)
{
    DvzCanvas* canvas = (DvzCanvas*)glfwGetWindowUserPointer(window);
    ASSERT(canvas != NULL);
    ASSERT(canvas->window != NULL);
    if (!canvas->mouse.is_active)
        return;

    // TODO: get modifier from canvas struct
    _enqueue_mouse_move(canvas, (vec2){xpos, ypos}, 0);
    // dvz_event_mouse_move(canvas, (vec2){xpos, ypos}, canvas->mouse.modifiers);
}

static void _glfw_frame_callback(DvzCanvas* canvas, DvzEvent ev)
{
    ASSERT(canvas != NULL);
    if (!canvas->mouse.is_active)
        return;
    GLFWwindow* w = canvas->window->backend_window;
    ASSERT(w != NULL);

    glm_vec2_copy(canvas->mouse.cur_pos, canvas->mouse.last_pos);

    // log_debug("mouse event %d", canvas->frame_idx);
    canvas->mouse.prev_state = canvas->mouse.cur_state;

    // Mouse move event.
    double xpos, ypos;
    glfwGetCursorPos(w, &xpos, &ypos);
    vec2 pos = {xpos, ypos};
    if (canvas->mouse.cur_pos[0] != pos[0] || canvas->mouse.cur_pos[1] != pos[1])
        dvz_event_mouse_move(canvas, pos, canvas->mouse.modifiers);

    // TODO
    // // Reset click events as soon as the next loop iteration after they were raised.
    // if (mouse->cur_state == DVZ_MOUSE_STATE_CLICK ||
    //     mouse->cur_state == DVZ_MOUSE_STATE_DOUBLE_CLICK)
    // {
    //     mouse->cur_state = DVZ_MOUSE_STATE_INACTIVE;
    //     mouse->button = DVZ_MOUSE_BUTTON_NONE;
    // }
}

static void _backend_next_frame(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);

    // Reset wheel event.
    if (canvas->mouse.cur_state == DVZ_MOUSE_STATE_WHEEL)
    {
        // log_debug("reset wheel state %d", canvas->frame_idx);
        canvas->mouse.cur_state = DVZ_MOUSE_STATE_INACTIVE;
    }
}

static void backend_event_callbacks(DvzCanvas* canvas)
{
    ASSERT(canvas != NULL);
    ASSERT(canvas->app != NULL);
    switch (canvas->app->backend)
    {
    case DVZ_BACKEND_GLFW:;
        if (canvas->window == NULL)
            return;
        GLFWwindow* w = canvas->window->backend_window;

        // The canvas pointer will be available to callback functions.
        glfwSetWindowUserPointer(w, canvas);

        // Register the key callback.
        glfwSetKeyCallback(w, _glfw_key_callback);

        // Register the mouse wheel callback.
        glfwSetScrollCallback(w, _glfw_wheel_callback);

        // Register the mouse button callback.
        glfwSetMouseButtonCallback(w, _glfw_button_callback);

        // Register the mouse move callback.
        // TODO: comment?? if commented, see _glfw_frame_callback
        glfwSetCursorPosCallback(w, _glfw_move_callback);

        // Register a function called at every frame, after event polling and state update
        dvz_event_callback(
            canvas, DVZ_EVENT_INTERACT, 0, DVZ_EVENT_MODE_SYNC, _glfw_frame_callback, NULL);

        break;
    default:
        break;
    }
}



#ifdef __cplusplus
}
#endif

#endif
