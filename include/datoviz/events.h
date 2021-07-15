#ifndef DVZ_EVENTS_HEADER
#define DVZ_EVENTS_HEADER

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



#ifdef __cplusplus
}
#endif

#endif
