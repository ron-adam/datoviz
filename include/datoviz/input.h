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
/*  Enums                                                                                        */
/*************************************************************************************************/

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

typedef struct DvzInputMouse DvzInputMouse;
typedef struct DvzInputKeyboard DvzInputKeyboard;
typedef struct DvzInputMouseLocal DvzInputMouseLocal;
typedef struct DvzInput DvzInput;



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



struct DvzInput
{
    DvzDeq deq;
    DvzInputMouse mouse;
    DvzInputKeyboard keyboard;
};



/*************************************************************************************************/
/*  Functions                                                                                    */
/*************************************************************************************************/

DVZ_EXPORT DvzInput dvz_input(DvzBackend backend, void* window);



#ifdef __cplusplus
}
#endif

#endif
