#include "../include/datoviz/input.h"
#include "backend_glfw.h"



/*************************************************************************************************/
/*  Input                                                                                        */
/*************************************************************************************************/

DvzInput dvz_input()
{
    DvzInput input = {0};

    // Two queues: mouse and keyboard.
    input.deq = dvz_deq(2);

    // Procs
    dvz_deq_proc(&input.deq, DVZ_INPUT_DEQ_MOUSE, 1, (uint32_t[]){DVZ_INPUT_DEQ_MOUSE});
    dvz_deq_proc(&input.deq, DVZ_INPUT_DEQ_KEYBOARD, 1, (uint32_t[]){DVZ_INPUT_DEQ_KEYBOARD});

    return input;
}



void dvz_input_backend(DvzInput* input, DvzBackend backend, void* window)
{
    ASSERT(input != NULL);
    input->backend = backend;
    input->window = window;

    // TODO: switch backend and register glfw-specific callbacks
}



static void _deq_callback(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    ASSERT(item != NULL);
    ASSERT(user_data != NULL);

    DvzInputCallbackPayload* payload = (DvzInputCallbackPayload*)user_data;
    DvzInput* input = payload->input;

    DvzInputEvent* ev = (DvzInputEvent*)item;

    payload->callback(input, *ev, input);

    return;
}

void dvz_input_callback(
    DvzInput* input, DvzInputType type, DvzInputCallback callback, void* user_data)
{
    ASSERT(input != NULL);

    // Find the Deq queue index by inspecting the input type used for registering the callback.
    uint32_t idx = DVZ_INPUT_DEQ_MOUSE;
    if (type == DVZ_INPUT_KEYBOARD_PRESS || type == DVZ_INPUT_KEYBOARD_RELEASE ||
        type == DVZ_INPUT_KEYBOARD_STROKE)
        idx = DVZ_INPUT_DEQ_KEYBOARD;

    ASSERT(input->callback_count < DVZ_INPUT_MAX_CALLBACKS);
    DvzInputCallbackPayload* payload = &input->callbacks[input->callback_count++];
    payload->input = input;
    payload->callback = callback;
    dvz_deq_callback(&input->deq, idx, (int)type, _deq_callback, payload);
}



void dvz_input_event(DvzInput* input, DvzInputType type, DvzInputEvent ev) {}
