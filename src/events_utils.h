#ifndef DVZ_EVENTS_UTILS_HEADER
#define DVZ_EVENTS_UTILS_HEADER

#include "../include/datoviz/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Canvas Deq callbacks                                                                         */
/*************************************************************************************************/

static void _canvas_to_refill(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    DvzCanvas* canvas = (DvzCanvas*)user_data;
    ASSERT(canvas != NULL);
    // TODO: set canvas->to_refill=true?
}



static void _canvas_to_close(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    DvzCanvas* canvas = (DvzCanvas*)user_data;
    ASSERT(canvas != NULL);
    // TODO: set canvas->to_close=true?
}



// Called when a mouse move event has been dequeued.
static void _canvas_mouse_move(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(deq != NULL);
    DvzCanvas* canvas = (DvzCanvas*)user_data;
    ASSERT(canvas != NULL);

    DvzMouseMoveEvent* ev = (DvzMouseMoveEvent*)item;
    ASSERT(ev != NULL);

    // TODO: update Mouse struct
    log_debug("mouse move to %.3fx%.3f", ev->pos[0], ev->pos[1]);

    // TODO: collapse other mouse move events?
}



#ifdef __cplusplus
}
#endif

#endif
