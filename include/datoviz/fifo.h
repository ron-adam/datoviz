/*************************************************************************************************/
/*  Standalone, thread-safe, generic FIFO queue                                                  */
/*************************************************************************************************/

#ifndef DVZ_FIFO_HEADER
#define DVZ_FIFO_HEADER

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_MAX_FIFO_CAPACITY 256
#define DVZ_DEQ_MAX_QUEUES    8
#define DVZ_DEQ_MAX_PROC_SIZE 4
#define DVZ_DEQ_MAX_PROCS     4
#define DVZ_DEQ_MAX_CALLBACKS 32



/*************************************************************************************************/
/*  Type definitions                                                                             */
/*************************************************************************************************/

typedef struct DvzFifo DvzFifo;
typedef struct DvzDeq DvzDeq;
typedef struct DvzDeqItem DvzDeqItem;
typedef struct DvzDeqProc DvzDeqProc;
typedef struct DvzDeqCallbackRegister DvzDeqCallbackRegister;

typedef void (*DvzDeqCallback)(DvzDeq* deq, void* item, void* user_data);



/*************************************************************************************************/
/*  FIFO queue                                                                                   */
/*************************************************************************************************/

struct DvzFifo
{
    int32_t tail, head;
    int32_t capacity;
    void** items;
    void* user_data;

    pthread_mutex_t lock;
    pthread_cond_t cond;

    atomic(bool, is_processing);
    atomic(bool, is_empty);
};



/*************************************************************************************************/
/*  Dequeues struct                                                                              */
/*************************************************************************************************/

struct DvzDeqCallbackRegister
{
    uint32_t deq_idx;
    int type;
    DvzDeqCallback callback;
    void* user_data;
};

struct DvzDeqItem
{
    uint32_t deq_idx;
    int type;
    void* item;
};

// A Proc represents a pair consumer/producer, where typically one thread enqueues items in a
// subset of the queues, and another thread dequeues items from that subset.
struct DvzDeqProc
{
    // Which queues constitute this process.
    uint32_t queue_count;
    uint32_t queue_indices[DVZ_DEQ_MAX_PROC_SIZE];
    uint32_t queue_offset; // offset that regularly increases at every call of dequeue()

    // Mutex and cond to signal when the deq is non-empty, and when to dequeue the first non-empty
    // underlying FIFO queues.
    pthread_mutex_t lock;
    pthread_cond_t cond;
    atomic(bool, is_processing);
};

struct DvzDeq
{
    uint32_t queue_count;
    DvzFifo queues[DVZ_DEQ_MAX_QUEUES];

    uint32_t callback_count;
    DvzDeqCallbackRegister callbacks[DVZ_DEQ_MAX_CALLBACKS];

    uint32_t proc_count;
    DvzDeqProc procs[DVZ_DEQ_MAX_PROCS];
    uint32_t q_to_proc[DVZ_DEQ_MAX_QUEUES]; // for each queue, which proc idx is handling it
};



/*************************************************************************************************/
/*  FIFO queue                                                                                   */
/*************************************************************************************************/

/**
 * Create a FIFO queue.
 *
 * @param capacity the maximum size
 * @returns a FIFO queue
 */
DVZ_EXPORT DvzFifo dvz_fifo(int32_t capacity);

/**
 * Enqueue an object in a queue.
 *
 * @param fifo the FIFO queue
 * @param item the pointer to the object to enqueue
 */
DVZ_EXPORT void dvz_fifo_enqueue(DvzFifo* fifo, void* item);

/**
 * Enqueue an object first in a queue.
 *
 * @param fifo the FIFO queue
 * @param item the pointer to the object to enqueue
 */
DVZ_EXPORT void dvz_fifo_enqueue_first(DvzFifo* fifo, void* item);

/**
 * Dequeue an object from a queue.
 *
 * @param fifo the FIFO queue
 * @param wait whether to return immediately, or wait until the queue is non-empty
 * @returns a pointer to the dequeued object, or NULL if the queue is empty
 */
DVZ_EXPORT void* dvz_fifo_dequeue(DvzFifo* fifo, bool wait);

/**
 * Get the number of items in a queue.
 *
 * @param fifo the FIFO queue
 * @returns the number of elements in the queue
 */
DVZ_EXPORT int dvz_fifo_size(DvzFifo* fifo);

/**
 * Wait until a FIFO queue is empty.
 *
 * @param fifo the FIFO queue
 */
DVZ_EXPORT void dvz_fifo_wait(DvzFifo* fifo);

/**
 * Discard old items in a queue.
 *
 * This function will suppress all items in the queue except the `max_size` most recent ones.
 *
 * @param fifo the FIFO queue
 * @param max_size the number of items to keep in the queue.
 */
DVZ_EXPORT void dvz_fifo_discard(DvzFifo* fifo, int max_size);

/**
 * Delete all items in a queue.
 *
 * @param fifo the FIFO queue
 */
DVZ_EXPORT void dvz_fifo_reset(DvzFifo* fifo);

/**
 * Destroy a queue.
 *
 * @param fifo the FIFO queue
 */
DVZ_EXPORT void dvz_fifo_destroy(DvzFifo* fifo);



/*************************************************************************************************/
/*  Dequeues                                                                                     */
/*************************************************************************************************/

/**
 * Create a Deq structure.
 *
 * A Deq is a set of dequeues, or double-ended queue. One can enqueue items in any of these queues,
 * and dequeue items.
 *
 * A item is defined by a pointer, a queue index, and a type (integer).
 *
 * The Deq is thread-safe.
 *
 * The Deq is multi-producer, single consumer. Multiple threads may enqueue items, but only a
 * single thread is supposed to dequeue items. That thread may also enqueue items.
 *
 * Function callbacks can be registered: they are called every time a item is dequeue. A callback
 * is defined by its queue index, and a item type. It will only be called for items that were
 * dequeued from the specified queue, if these items have the appropriate type.
 *
 * A Proc represents a pair of "processes" (to be understood in the general sense, not OS
 * processes), with a producer and a consumer. It is defined by a subset of the queues, which are
 * supposed to be dequeued from the item dequeueing loop (typically in a dedicated thread).
 *
 * @param capacity the maximum size
 * @returns a Deq
 */
DVZ_EXPORT DvzDeq dvz_deq(uint32_t nq);

/**
 * Define a callback.
 *
 * @param deq the Deq
 * @param deq_idx the queue index
 * @param type the type to register the callback to
 * @param user_data pointer to arbitrary data to be passed to the callback
 */
DVZ_EXPORT void dvz_deq_callback(
    DvzDeq* deq, uint32_t deq_idx, int type, DvzDeqCallback callback, void* user_data);

/**
 * Define a Proc.
 *
 * @param deq the Deq
 * @param proc_idx the Proc index (should be regularly increasing: 0, 1, 2, ...)
 * @param queue_count the number of queues in the Proc
 * @param queue_ids the indices of the queues in the Proc
 */
DVZ_EXPORT void
dvz_deq_proc(DvzDeq* deq, uint32_t proc_idx, uint32_t queue_count, uint32_t* queue_ids);

/**
 * Enqueue an item.
 *
 * @param deq the Deq
 * @param deq_idx the queue index
 * @param type the item type
 * @param item a pointer to the item
 */
DVZ_EXPORT void dvz_deq_enqueue(DvzDeq* deq, uint32_t deq_idx, int type, void* item);

/**
 * Enqueue an item at the first position.
 *
 * @param deq the Deq
 * @param deq_idx the queue index
 * @param type the item type
 * @param item a pointer to the item
 */
DVZ_EXPORT void dvz_deq_enqueue_first(DvzDeq* deq, uint32_t deq_idx, int type, void* item);

/**
 * Delete a number of items in a given queue.
 *
 * @param deq the Deq
 * @param deq_idx the queue index
 * @param max_size the maximum number of items to delete
 */
DVZ_EXPORT void dvz_deq_discard(DvzDeq* deq, uint32_t deq_idx, int max_size);

/**
 * Return the first item item in a given queue.
 *
 * @param deq the Deq
 * @param deq_idx the queue index
 * @returns the item
 */
DVZ_EXPORT DvzDeqItem dvz_deq_peek_first(DvzDeq* deq, uint32_t deq_idx);

/**
 * Return thea last item item in a given queue.
 *
 * @param deq the Deq
 * @param deq_idx the queue index
 * @returns the item
 */
DVZ_EXPORT DvzDeqItem dvz_deq_peek_last(DvzDeq* deq, uint32_t deq_idx);

/**
 * Dequeue a non-empty item from one of the queues of a given proc.
 *
 * @param deq the Deq
 * @param proc_idx the Proc index
 * @param wait whether this call should be blocking
 */
DVZ_EXPORT DvzDeqItem dvz_deq_dequeue(DvzDeq* deq, uint32_t proc_idx, bool wait);

/**
 * Wait until all queues within a given Proc are empty.
 *
 * @param deq the Deq
 * @param proc_idx the Proc index
 */
DVZ_EXPORT void dvz_deq_wait(DvzDeq* deq, uint32_t proc_idx);

/**
 * Destroy a Deq.
 *
 * @param deq the Deq
 */
DVZ_EXPORT void dvz_deq_destroy(DvzDeq* deq);



static void* _deq_loop(DvzDeq* deq, uint32_t proc_idx)
{
    ASSERT(deq != NULL);
    ASSERT(proc_idx < deq->proc_count);
    DvzDeqItem item = {0};

    while (true)
    {
        log_trace("waiting for proc #%d", proc_idx);
        // This call dequeues an item and also calls all registered callbacks if the item is not
        // null.
        item = dvz_deq_dequeue(deq, proc_idx, true);
        if (item.item == NULL)
        {
            log_debug("stop the deq loop for proc #%d", proc_idx);
            break;
        }
        else
        {
            // TODO: special callbacks

            // WARNING: the pointer MUST be alloc-ed on the heap, because it is always
            // freed here after dequeue and callbacks.
            log_trace("free item");
            FREE(item.item);
        }
        log_trace("got a deq item on proc #%d", proc_idx);
    }
    return NULL;
}



#ifdef __cplusplus
}
#endif

#endif
