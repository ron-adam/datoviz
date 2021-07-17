/*************************************************************************************************/
/*  Input system (mouse, keyboard)                                                               */
/*************************************************************************************************/

#ifndef DVZ_INPUT_HEADER
#define DVZ_INPUT_HEADER

#include "app.h"
#include "common.h"
#include "fifo.h"
#include "keycode.h"



#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_INPUT_DEQ_MOUSE    0
#define DVZ_INPUT_DEQ_KEYBOARD 1

#define DVZ_INPUT_MAX_CALLBACKS 64



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// Input type.
typedef enum
{
    DVZ_INPUT_NONE,

    DVZ_INPUT_MOUSE_MOVE,
    DVZ_INPUT_MOUSE_PRESS,
    DVZ_INPUT_MOUSE_RELEASE,
    DVZ_INPUT_MOUSE_CLICK,
    DVZ_INPUT_MOUSE_DOUBLE_CLICK,
    DVZ_INPUT_MOUSE_WHEEL,
    DVZ_INPUT_MOUSE_DRAG_BEGIN,
    DVZ_INPUT_MOUSE_DRAG,
    DVZ_INPUT_MOUSE_DRAG_END,

    DVZ_INPUT_KEYBOARD_PRESS,
    DVZ_INPUT_KEYBOARD_RELEASE,
    DVZ_INPUT_KEYBOARD_STROKE,

} DvzInputType;



// Key modifiers
// NOTE: must match GLFW values! no mapping is done for now
typedef enum
{
    DVZ_KEY_MODIFIER_NONE = 0x00000000,
    DVZ_KEY_MODIFIER_SHIFT = 0x00000001,
    DVZ_KEY_MODIFIER_CONTROL = 0x00000002,
    DVZ_KEY_MODIFIER_ALT = 0x00000004,
    DVZ_KEY_MODIFIER_SUPER = 0x00000008,
} DvzKeyModifiers;



// Mouse button
typedef enum
{
    DVZ_MOUSE_BUTTON_NONE,
    DVZ_MOUSE_BUTTON_LEFT,
    DVZ_MOUSE_BUTTON_MIDDLE,
    DVZ_MOUSE_BUTTON_RIGHT,
} DvzMouseButton;



// Mouse state type
typedef enum
{
    DVZ_MOUSE_STATE_INACTIVE,
    DVZ_MOUSE_STATE_DRAG,
    DVZ_MOUSE_STATE_WHEEL,
    DVZ_MOUSE_STATE_CLICK,
    DVZ_MOUSE_STATE_DOUBLE_CLICK,
    DVZ_MOUSE_STATE_CAPTURE,
} DvzMouseStateType;



// Key state type
typedef enum
{
    DVZ_KEYBOARD_STATE_INACTIVE,
    DVZ_KEYBOARD_STATE_ACTIVE,
    DVZ_KEYBOARD_STATE_CAPTURE,
} DvzKeyboardStateType;



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct DvzKeyEvent DvzKeyEvent;
typedef struct DvzMouseButtonEvent DvzMouseButtonEvent;
typedef struct DvzMouseClickEvent DvzMouseClickEvent;
typedef struct DvzMouseDragEvent DvzMouseDragEvent;
typedef struct DvzMouseMoveEvent DvzMouseMoveEvent;
typedef struct DvzMouseWheelEvent DvzMouseWheelEvent;

typedef struct DvzInputMouse DvzInputMouse;
typedef struct DvzInputKeyboard DvzInputKeyboard;
typedef struct DvzInputMouseLocal DvzInputMouseLocal;
typedef union DvzInputEvent DvzInputEvent;
typedef struct DvzInput DvzInput;

typedef struct DvzInputCallbackPayload DvzInputCallbackPayload;
typedef void (*DvzInputCallback)(DvzInput*, DvzInputEvent, void*);



/*************************************************************************************************/
/*  Event structs                                                                                */
/*************************************************************************************************/

struct DvzMouseButtonEvent
{
    DvzMouseButton button;
    int modifiers;
};



struct DvzMouseMoveEvent
{
    vec2 pos;
    int modifiers;
};



struct DvzMouseWheelEvent
{
    vec2 pos; // TODO: remove
    vec2 dir;
    int modifiers;
};



struct DvzMouseDragEvent
{
    vec2 pos;
    DvzMouseButton button;
    int modifiers;
};



struct DvzMouseClickEvent
{
    vec2 pos;
    DvzMouseButton button;
    int modifiers;
    bool double_click;
};



struct DvzKeyEvent
{
    DvzKeyCode key_code;
    int modifiers;
};



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct DvzInputMouse
{
    DvzMouseButton button;
    vec2 press_pos;
    vec2 last_pos;
    vec2 cur_pos;
    vec2 wheel_delta;
    float shift_length;
    int modifiers;

    DvzMouseStateType prev_state;
    DvzMouseStateType cur_state;

    double press_time;
    double click_time;
    bool is_active;
};



union DvzInputEvent
{
    DvzKeyEvent k;         // for KEY events
    DvzMouseButtonEvent b; // for BUTTON events
    DvzMouseClickEvent c;  // for CLICK events
    DvzMouseDragEvent d;   // for DRAG events
    DvzMouseMoveEvent m;   // for MOVE events
    DvzMouseWheelEvent w;  // for WHEEL events
};



// In normalize coordinates [-1, +1]
struct DvzInputMouseLocal
{
    vec2 press_pos;
    vec2 last_pos;
    vec2 cur_pos;
    // vec2 delta; // delta between the last and current pos
    // vec2 press_delta; // delta between t
};



struct DvzInputKeyboard
{
    DvzKeyCode key_code;
    int modifiers;

    DvzKeyboardStateType prev_state;
    DvzKeyboardStateType cur_state;

    double press_time;
    bool is_active;
};



struct DvzInputCallbackPayload
{
    DvzInput* input;
    DvzInputCallback callback;
    void* user_data;
};



struct DvzInput
{
    DvzBackend backend;
    DvzDeq deq;
    DvzInputMouse mouse;
    DvzInputKeyboard keyboard;

    uint32_t callback_count;
    DvzInputCallbackPayload callbacks[DVZ_INPUT_MAX_CALLBACKS];
    DvzThread thread; // background thread processing the input events
    DvzClock clock;

    void* window;
};



/*************************************************************************************************/
/*  Functions                                                                                    */
/*************************************************************************************************/

/**
 * Create an input struct.
 *
 * An Input provides an event queue for mouse and keyboard events. It also attaches a backend
 * window and fills the event queue as a response to user input events.
 *
 * The Input also allows the user to attach a callback function to react to these events.
 *
 * Finally, the Input provides a way to enqueue input events directly in it, thereby simulating
 * mock mouse and keyboard events.
 *
 * @returns the input struct
 */
DVZ_EXPORT DvzInput dvz_input(void);

/**
 * Setup an input for a given backend and window object.
 *
 * @param input the input
 * @param backend the backend
 * @param window the backend-specific window object
 */
DVZ_EXPORT void dvz_input_backend(DvzInput* input, DvzBackend backend, void* window);

/**
 * Register an input callback.
 *
 * @param input the input
 * @param type the input type
 * @param callback the callback function
 * @param user_data pointer to arbitrary data
 */
DVZ_EXPORT void
dvz_input_callback(DvzInput* input, DvzInputType type, DvzInputCallback callback, void* user_data);

/**
 * Enqueue an input event.
 *
 * @param input the input
 * @param type the input type
 * @param ev the event union
 */
DVZ_EXPORT void dvz_input_event(DvzInput* input, DvzInputType type, DvzInputEvent ev);

/**
 * Enqueue an input event at the first end of the queue.
 *
 * @param input the input
 * @param type the input type
 * @param ev the event union
 */
DVZ_EXPORT void dvz_input_event_first(DvzInput* input, DvzInputType type, DvzInputEvent ev);

/**
 * Destroy an input struct.
 * @param input the input struct
 */
DVZ_EXPORT void dvz_input_destroy(DvzInput* input);



/**
 * Create the mouse object holding the current mouse state.
 *
 * @returns mouse object
 */
DVZ_EXPORT DvzInputMouse dvz_input_mouse(void);

/**
 * Active or deactivate interactive mouse events.
 *
 * @param mouse the mouse object
 * @param enable whether to activate or deactivate mouse events
 */
DVZ_EXPORT void dvz_input_mouse_toggle(DvzInputMouse* mouse, bool enable);

/**
 * Reset the mouse state.
 *
 * @param mouse the mouse object
 */
DVZ_EXPORT void dvz_input_mouse_reset(DvzInputMouse* mouse);

/**
 * Emit a mouse event.
 *
 * @param mouse the input
 * @param type the event type
 * @param ev the mouse event
 */
DVZ_EXPORT void dvz_input_mouse_update(DvzInput* input, DvzInputType type, DvzInputEvent* ev);



#ifdef __cplusplus
}
#endif

#endif
