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
#define DVZ_INPUT_DEQ_TIMER    2

#define DVZ_INPUT_MAX_CALLBACKS 64
#define DVZ_INPUT_MAX_KEYS      8
#define DVZ_INPUT_MAX_TIMERS    16



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
    DVZ_INPUT_KEYBOARD_STROKE, // unused for now

    DVZ_INPUT_TIMER_ADD,
    DVZ_INPUT_TIMER_RUNNING,
    DVZ_INPUT_TIMER_TICK,
    DVZ_INPUT_TIMER_REMOVE,

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

typedef struct DvzTimerAddEvent DvzTimerAddEvent;
typedef struct DvzTimerRunningEvent DvzTimerRunningEvent;
typedef struct DvzTimerRemoveEvent DvzTimerRemoveEvent;
typedef struct DvzTimerTickEvent DvzTimerTickEvent;

typedef struct DvzInputMouse DvzInputMouse;
typedef struct DvzInputKeyboard DvzInputKeyboard;
typedef struct DvzInputMouseLocal DvzInputMouseLocal;
typedef union DvzInputEvent DvzInputEvent;
typedef struct DvzInput DvzInput;

typedef struct DvzTimer DvzTimer;

typedef struct DvzInputCallbackPayload DvzInputCallbackPayload;
typedef void (*DvzInputCallback)(DvzInput*, DvzInputEvent, void*);



/*************************************************************************************************/
/*  Mouse event structs                                                                          */
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



/*************************************************************************************************/
/*  Keyboard event structs                                                                       */
/*************************************************************************************************/

struct DvzKeyEvent
{
    DvzKeyCode key_code;
    int modifiers;
};



/*************************************************************************************************/
/*  Timer event structs                                                                          */
/*************************************************************************************************/

struct DvzTimerAddEvent
{
    uint32_t timer_id;  // which timer
    uint64_t max_count; // maximum number of iterations
    double after;       // after how many seconds the first event should be raised
    double period;      // period of the associated timer
};



struct DvzTimerRunningEvent
{
    uint32_t timer_id; // which timer
    bool is_running;   // false=pause, true=continue
};



struct DvzTimerRemoveEvent
{
    uint32_t timer_id; // which timer
};



struct DvzTimerTickEvent
{
    uint32_t timer_id;  // which timer
    uint64_t tick;      // increasing at every event emission
    uint64_t max_count; // maximum number of iterations
    double after;       // after how many seconds the first event should be raised
    double period;      // period of the associated timer
    double time;        // current time
    double interval;    // interval since last event emission
};



/*************************************************************************************************/
/*  Input event union                                                                            */
/*************************************************************************************************/

union DvzInputEvent
{
    DvzKeyEvent k;         // for KEY events
    DvzMouseButtonEvent b; // for BUTTON events
    DvzMouseClickEvent c;  // for CLICK events
    DvzMouseDragEvent d;   // for DRAG events
    DvzMouseMoveEvent m;   // for MOVE events
    DvzMouseWheelEvent w;  // for WHEEL events

    DvzTimerTickEvent t;     // for TIMER events
    DvzTimerAddEvent ta;     // for TIMER_ADD events
    DvzTimerRunningEvent tp; // for TIMER_RUNNING events (pause/continue)
    DvzTimerRemoveEvent tr;  // for TIMER_REMOVE events
};



/*************************************************************************************************/
/*  Mouse structs                                                                                */
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



// In normalize coordinates [-1, +1]
struct DvzInputMouseLocal
{
    vec2 press_pos;
    vec2 last_pos;
    vec2 cur_pos;
    // vec2 delta; // delta between the last and current pos
    // vec2 press_delta; // delta between t
};



/*************************************************************************************************/
/*  Keyboard structs                                                                             */
/*************************************************************************************************/

struct DvzInputKeyboard
{
    uint32_t key_count;                  // number of keys currently pressed
    DvzKeyCode keys[DVZ_INPUT_MAX_KEYS]; // which keys are currently pressed
    int modifiers;

    DvzKeyboardStateType prev_state;
    DvzKeyboardStateType cur_state;

    double press_time;
    bool is_active;
};



/*************************************************************************************************/
/*  Timer structs                                                                                */
/*************************************************************************************************/

struct DvzTimer
{
    DvzObject obj;
    DvzInput* input;
    bool is_running;

    uint32_t timer_id;  // unique ID of this timer among all timers registered in a given input
    uint64_t tick;      // current tick number
    uint64_t max_count; // specified maximum number of ticks for this timer
    double after;       // number of seconds before the first tick
    double period;      // expected number of seconds between ticks
    double interval;    // number of seconds since last tick
    DvzInputCallback callback;
};



/*************************************************************************************************/
/*  Input structs                                                                                */
/*************************************************************************************************/

struct DvzInputCallbackPayload
{
    DvzInput* input;
    DvzInputCallback callback;
    void* user_data;
};



struct DvzInput
{
    DvzBackend backend;

    // Event queues for mouse, keyboard, and timer.
    DvzDeq deq;

    // Mouse and keyboard.
    DvzInputMouse mouse;
    DvzInputKeyboard keyboard;

    // Timers.
    // uint32_t timer_count;
    // DvzTimer timers[DVZ_INPUT_MAX_TIMERS];
    DvzContainer timers;

    // Callbacks.
    uint32_t callback_count;
    DvzInputCallbackPayload callbacks[DVZ_INPUT_MAX_CALLBACKS];

    // Thread and clock.
    DvzThread thread; // background thread processing the input events
    DvzClock clock;

    // Pointer to a native backend window object.
    void* window;
};



/*************************************************************************************************/
/*  Mouse                                                                                        */
/*************************************************************************************************/

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
 * Update the mouse state after every mouse event.
 *
 * @param mouse the input
 * @param type the event type
 * @param ev the mouse event
 */
DVZ_EXPORT void dvz_input_mouse_update(DvzInput* input, DvzInputType type, DvzInputEvent* ev);



/*************************************************************************************************/
/*  Keyboard                                                                                     */
/*************************************************************************************************/

/**
 * Create the keyboard object holding the current keyboard state.
 *
 * @returns keyboard object
 */
DVZ_EXPORT DvzInputKeyboard dvz_input_keyboard(void);

/**
 * Active or deactivate interactive keyboard events.
 *
 * @param keyboard the keyboard object
 * @param enable whether to activate or deactivate keyboard events
 */
DVZ_EXPORT void dvz_input_keyboard_toggle(DvzInputKeyboard* keyboard, bool enable);

/**
 * Reset the keyboard state
 *
 * @returns keyboard object
 */
DVZ_EXPORT void dvz_input_keyboard_reset(DvzInputKeyboard* keyboard);

/**
 * Update the keyboard state after every mouse event.
 *
 * @param keyboard the input
 * @param type the event type
 * @param ev the keyboard event
 */
DVZ_EXPORT void dvz_input_keyboard_update(DvzInput* input, DvzInputType type, DvzInputEvent* ev);



/*************************************************************************************************/
/*  Timer                                                                                        */
/*************************************************************************************************/

// /**
//  * Create a timer.
//  *
//  * @returns the timer
//  */
// DVZ_EXPORT DvzTimer*
// dvz_input_timer(DvzInput* input, double after, double period, uint64_t max_count);

// /**
//  * Pause or resume the timer.
//  */
// DVZ_EXPORT void dvz_input_timer_running(DvzTimer* timer, bool is_running);

// /**
//  * Remove a timer.
//  */
// DVZ_EXPORT void dvz_input_timer_remove(DvzTimer* timer);



/*************************************************************************************************/
/*  Input functions                                                                              */
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



#ifdef __cplusplus
}
#endif

#endif
