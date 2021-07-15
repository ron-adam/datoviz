#include "../include/datoviz/fifo.h"



/*************************************************************************************************/
/*  Thread-safe FIFO queue                                                                       */
/*************************************************************************************************/

DvzFifo dvz_fifo(int32_t capacity)
{
    log_trace("creating generic FIFO queue with a capacity of %d items", capacity);
    ASSERT(capacity >= 2);
    DvzFifo fifo = {0};
    ASSERT(capacity <= DVZ_MAX_FIFO_CAPACITY);
    fifo.capacity = capacity;
    fifo.is_empty = true;
    fifo.items = calloc((uint32_t)capacity, sizeof(void*));

    if (pthread_mutex_init(&fifo.lock, NULL) != 0)
        log_error("mutex creation failed");
    if (pthread_cond_init(&fifo.cond, NULL) != 0)
        log_error("cond creation failed");

    return fifo;
}



static void _fifo_resize(DvzFifo* fifo)
{
    // Old size
    int size = fifo->tail - fifo->head;
    if (size < 0)
        size += fifo->capacity;

    // Old capacity
    int old_cap = fifo->capacity;

    // Resize if queue is full.
    if ((fifo->tail + 1) % fifo->capacity == fifo->head)
    {
        ASSERT(fifo->items != NULL);
        ASSERT(size == fifo->capacity - 1);
        ASSERT(fifo->capacity <= DVZ_MAX_FIFO_CAPACITY);

        fifo->capacity *= 2;
        log_debug("FIFO queue is full, enlarging it to %d", fifo->capacity);
        REALLOC(fifo->items, (uint32_t)fifo->capacity * sizeof(void*));
    }

    if ((fifo->tail + 1) % fifo->capacity == fifo->head)
    {
        // Here, the queue buffer has been resized, but the new space should be used instead of the
        // part of the buffer before the head.

        ASSERT(fifo->tail > 0);
        ASSERT(old_cap < fifo->capacity);
        memcpy(&fifo->items[old_cap], &fifo->items[0], (uint32_t)fifo->tail * sizeof(void*));

        // Move the tail to the new position.
        fifo->tail += old_cap;

        // Check new size.
        ASSERT(fifo->tail - fifo->head > 0);
        ASSERT(fifo->tail - fifo->head == size);
    }
}



void dvz_fifo_enqueue(DvzFifo* fifo, void* item)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);

    // Resize the FIFO queue if needed.
    _fifo_resize(fifo);

    ASSERT((fifo->tail + 1) % fifo->capacity != fifo->head);
    fifo->items[fifo->tail] = item;
    fifo->tail++;
    if (fifo->tail >= fifo->capacity)
        fifo->tail -= fifo->capacity;
    fifo->is_empty = false;

    ASSERT(0 <= fifo->tail && fifo->tail < fifo->capacity);
    pthread_cond_signal(&fifo->cond);
    pthread_mutex_unlock(&fifo->lock);
}



void dvz_fifo_enqueue_first(DvzFifo* fifo, void* item)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);

    // Resize the FIFO queue if needed.
    _fifo_resize(fifo);

    ASSERT((fifo->tail + 1) % fifo->capacity != fifo->head);
    fifo->head--;
    if (fifo->head < 0)
        fifo->head += fifo->capacity;
    ASSERT(0 <= fifo->head && fifo->head < fifo->capacity);

    fifo->items[fifo->head] = item;
    fifo->is_empty = false;

    ASSERT(0 <= fifo->tail && fifo->tail < fifo->capacity);
    int size = fifo->tail - fifo->head;
    if (size < 0)
        size += fifo->capacity;
    ASSERT(0 <= size && size < fifo->capacity);

    pthread_cond_signal(&fifo->cond);
    pthread_mutex_unlock(&fifo->lock);
}



void* dvz_fifo_dequeue(DvzFifo* fifo, bool wait)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);

    // Wait until the queue is not empty.
    if (wait)
    {
        log_trace("waiting for the queue to be non-empty");
        while (fifo->tail == fifo->head)
            // NOTE: this call automatically releases the mutex while waiting, and reacquires it
            // afterwards
            pthread_cond_wait(&fifo->cond, &fifo->lock);
    }

    // Empty queue.
    if (fifo->tail == fifo->head)
    {
        // log_trace("FIFO queue was empty");
        // Don't forget to unlock the mutex before exiting this function.
        pthread_mutex_unlock(&fifo->lock);
        fifo->is_empty = true;
        return NULL;
    }

    ASSERT(0 <= fifo->head && fifo->head < fifo->capacity);

    // log_trace("dequeue item, tail %d, head %d", fifo->tail, fifo->head);
    void* item = fifo->items[fifo->head];

    fifo->head++;
    if (fifo->head >= fifo->capacity)
        fifo->head -= fifo->capacity;

    ASSERT(0 <= fifo->head && fifo->head < fifo->capacity);

    if (fifo->tail == fifo->head)
        fifo->is_empty = true;

    pthread_mutex_unlock(&fifo->lock);
    return item;
}



int dvz_fifo_size(DvzFifo* fifo)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);
    // log_debug("tail %d head %d", fifo->tail, fifo->head);
    int size = fifo->tail - fifo->head;
    if (size < 0)
        size += fifo->capacity;
    ASSERT(0 <= size && size <= fifo->capacity);
    pthread_mutex_unlock(&fifo->lock);
    return size;
}



void dvz_fifo_wait(DvzFifo* fifo)
{
    ASSERT(fifo != NULL);
    while (dvz_fifo_size(fifo) > 0)
        dvz_sleep(1);
}



void dvz_fifo_discard(DvzFifo* fifo, int max_size)
{
    ASSERT(fifo != NULL);
    if (max_size == 0)
        return;
    pthread_mutex_lock(&fifo->lock);
    int size = fifo->tail - fifo->head;
    if (size < 0)
        size += fifo->capacity;
    ASSERT(0 <= size && size <= fifo->capacity);
    if (size > max_size)
    {
        log_trace(
            "discarding %d items in the FIFO queue which is getting overloaded", size - max_size);
        fifo->head = fifo->tail - max_size;
        if (fifo->head < 0)
            fifo->head += fifo->capacity;
    }
    pthread_mutex_unlock(&fifo->lock);
}



void dvz_fifo_reset(DvzFifo* fifo)
{
    ASSERT(fifo != NULL);
    pthread_mutex_lock(&fifo->lock);
    fifo->tail = 0;
    fifo->head = 0;
    pthread_cond_signal(&fifo->cond);
    pthread_mutex_unlock(&fifo->lock);
}



void dvz_fifo_destroy(DvzFifo* fifo)
{
    ASSERT(fifo != NULL);
    pthread_mutex_destroy(&fifo->lock);
    pthread_cond_destroy(&fifo->cond);

    ASSERT(fifo->items != NULL);
    FREE(fifo->items);
}



/*************************************************************************************************/
/*  Dequeues                                                                                     */
/*************************************************************************************************/

static DvzFifo* _deq_fifo(DvzDeq* deq, uint32_t deq_idx)
{
    ASSERT(deq != NULL);
    ASSERT(deq_idx < deq->queue_count);

    DvzFifo* fifo = &deq->queues[deq_idx];
    ASSERT(fifo != NULL);
    ASSERT(fifo->capacity > 0);
    return fifo;
}



// Call all callback functions registered with a deq_idx and type on a deq item.
static void _deq_callbacks(DvzDeq* deq, DvzDeqItem item_s)
{
    ASSERT(deq != NULL);
    ASSERT(item_s.item != NULL);
    DvzDeqCallbackRegister* reg = NULL;
    for (uint32_t i = 0; i < deq->callback_count; i++)
    {
        reg = &deq->callbacks[i];
        ASSERT(reg != NULL);
        if (reg->deq_idx == item_s.deq_idx && reg->type == item_s.type)
        {
            reg->callback(deq, item_s.item, reg->user_data);
        }
    }
}



DvzDeq dvz_deq(uint32_t nq)
{
    DvzDeq deq = {0};
    ASSERT(nq <= DVZ_DEQ_MAX_QUEUES);
    deq.queue_count = nq;
    for (uint32_t i = 0; i < nq; i++)
        deq.queues[i] = dvz_fifo(DVZ_MAX_FIFO_CAPACITY);
    return deq;
}



void dvz_deq_callback(
    DvzDeq* deq, uint32_t deq_idx, int type, DvzDeqCallback callback, void* user_data)
{
    ASSERT(deq != NULL);
    ASSERT(callback != NULL);

    DvzDeqCallbackRegister* reg = &deq->callbacks[deq->callback_count++];
    ASSERT(reg != NULL);

    reg->deq_idx = deq_idx;
    reg->type = type;
    reg->callback = callback;
    reg->user_data = user_data;
}



void dvz_deq_proc(DvzDeq* deq, uint32_t proc_idx, uint32_t queue_count, uint32_t* queue_ids)
{
    ASSERT(deq != NULL);
    ASSERT(queue_ids != NULL);

    // HACK: calls to dvz_deq_proc(deq, proc_idx, ...) must be with proc_idx strictly increasing:
    // 0, 1, 2...
    ASSERT(proc_idx == deq->proc_count);

    DvzDeqProc* proc = &deq->procs[deq->proc_count++];
    ASSERT(proc != NULL);

    ASSERT(queue_count <= DVZ_DEQ_MAX_PROC_SIZE);
    proc->queue_count = queue_count;
    // Copy the queue ids to the DvzDeqProc struct.
    for (uint32_t i = 0; i < queue_count; i++)
    {
        ASSERT(queue_ids[i] < deq->queue_count);
        proc->queue_indices[i] = queue_ids[i];

        // Register, for each of the indicated queue, which proc idx is handling it.
        ASSERT(queue_ids[i] < DVZ_DEQ_MAX_QUEUES);
        deq->q_to_proc[queue_ids[i]] = proc_idx;
    }

    // Initialize the thread objects.
    if (pthread_mutex_init(&proc->lock, NULL) != 0)
        log_error("mutex creation failed");
    if (pthread_cond_init(&proc->cond, NULL) != 0)
        log_error("cond creation failed");
    atomic_init(&proc->is_processing, false);
}



static void _deq_enqueue(DvzDeq* deq, uint32_t deq_idx, int type, void* item, bool enqueue_first)
{
    ASSERT(deq != NULL);
    ASSERT(deq_idx < deq->queue_count);
    ASSERT(deq_idx < DVZ_DEQ_MAX_QUEUES);

    DvzFifo* fifo = _deq_fifo(deq, deq_idx);
    DvzDeqItem* deq_item = calloc(1, sizeof(DvzDeqItem));
    ASSERT(deq_item != NULL);
    deq_item->deq_idx = deq_idx;
    deq_item->type = type;
    deq_item->item = item;

    // Find the proc that processes the specified queue.
    uint32_t proc_idx = deq->q_to_proc[deq_idx];
    ASSERT(proc_idx < deq->proc_count);
    DvzDeqProc* proc = &deq->procs[proc_idx];

    // We signal that proc that an item has been enqueued to one of its queues.
    log_trace("enqueue to queue #%d item type %d", deq_idx, type);
    pthread_mutex_lock(&proc->lock);
    if (!enqueue_first)
        dvz_fifo_enqueue(fifo, deq_item);
    else
        dvz_fifo_enqueue_first(fifo, deq_item);
    log_trace("signal cond of proc #%d", proc_idx);
    pthread_cond_signal(&proc->cond);
    pthread_mutex_unlock(&proc->lock);
}



void dvz_deq_enqueue(DvzDeq* deq, uint32_t deq_idx, int type, void* item)
{
    _deq_enqueue(deq, deq_idx, type, item, false);
}



void dvz_deq_enqueue_first(DvzDeq* deq, uint32_t deq_idx, int type, void* item)
{
    _deq_enqueue(deq, deq_idx, type, item, true);
}



void dvz_deq_discard(DvzDeq* deq, uint32_t deq_idx, int max_size)
{
    ASSERT(deq != NULL);
    ASSERT(deq_idx < deq->queue_count);
    DvzFifo* fifo = _deq_fifo(deq, deq_idx);
    dvz_fifo_discard(fifo, max_size);
}



DvzDeqItem dvz_deq_peek_first(DvzDeq* deq, uint32_t deq_idx)
{
    ASSERT(deq != NULL);
    ASSERT(deq_idx < deq->queue_count);
    DvzFifo* fifo = _deq_fifo(deq, deq_idx);
    return *((DvzDeqItem*)(fifo->items[fifo->head]));
}



DvzDeqItem dvz_deq_peek_last(DvzDeq* deq, uint32_t deq_idx)
{
    ASSERT(deq != NULL);
    ASSERT(deq_idx < deq->queue_count);
    DvzFifo* fifo = _deq_fifo(deq, deq_idx);
    int32_t last = fifo->tail - 1;
    if (last < 0)
        last += fifo->capacity;
    ASSERT(0 <= last && last < fifo->capacity);
    return *((DvzDeqItem*)(fifo->items[last]));
}



// Return the total size of the Deq.
static int _deq_size(DvzDeq* deq, uint32_t queue_count, uint32_t* queue_ids)
{
    ASSERT(deq != NULL);
    ASSERT(queue_count > 0);
    ASSERT(queue_ids != NULL);
    int size = 0;
    uint32_t deq_idx = 0;
    for (uint32_t i = 0; i < queue_count; i++)
    {
        deq_idx = queue_ids[i];
        ASSERT(deq_idx < deq->queue_count);
        size += dvz_fifo_size(&deq->queues[deq_idx]);
    }
    return size;
}

DvzDeqItem dvz_deq_dequeue(DvzDeq* deq, uint32_t proc_idx, bool wait)
{
    ASSERT(deq != NULL);

    DvzFifo* fifo = NULL;
    DvzDeqItem* deq_item = NULL;
    DvzDeqItem item_s = {0};

    ASSERT(proc_idx < deq->proc_count);
    DvzDeqProc* proc = &deq->procs[proc_idx];

    pthread_mutex_lock(&proc->lock);

    // Wait until the queue is not empty.
    if (wait)
    {
        log_trace("waiting for one of the queues in proc #%d to be non-empty", proc_idx);
        while (_deq_size(deq, proc->queue_count, proc->queue_indices) == 0)
        {
            log_trace("waiting for proc #%d cond", proc_idx);
            // NOTE: this call automatically releases the mutex while waiting, and reacquires it
            // afterwards
            pthread_cond_wait(&proc->cond, &proc->lock);
            log_trace("proc #%d cond signaled!", proc_idx);
        }
        log_trace("proc #%d has an item", proc_idx);
    }

    // Go through the passed queue indices.
    uint32_t deq_idx = 0;
    for (uint32_t i = 0; i < proc->queue_count; i++)
    {
        // This is the ID of the queue.
        deq_idx = proc->queue_indices[i];
        ASSERT(deq_idx < deq->queue_count);

        // Get that FIFO queue.
        fifo = _deq_fifo(deq, deq_idx);

        // Dequeue it immediately, return NULL if the queue was empty.
        deq_item = dvz_fifo_dequeue(fifo, false);
        if (deq_item != NULL)
        {
            // Make a copy of the struct.
            item_s = *deq_item;
            // Consistency check.
            ASSERT(deq_idx == item_s.deq_idx);
            log_trace("dequeue item from FIFO queue #%d with type %d", deq_idx, item_s.type);
            FREE(deq_item);
            break;
        }
        log_trace("queue #%d was empty", deq_idx);
    }
    // NOTE: we must unlock BEFORE calling the callbacks if we want to permit callbacks to enqueue
    // new tasks.
    pthread_mutex_unlock(&proc->lock);

    // Call the associated callbacks automatically.
    if (item_s.item != NULL)
    {
        atomic_store(&proc->is_processing, true);
        _deq_callbacks(deq, item_s);
    }

    atomic_store(&proc->is_processing, false);
    return item_s;
}



void dvz_deq_wait(DvzDeq* deq, uint32_t proc_idx)
{
    ASSERT(deq != NULL);

    ASSERT(proc_idx < deq->proc_count);
    DvzDeqProc* proc = &deq->procs[proc_idx];
    log_trace("start waiting for proc #%d", proc_idx);

    while (_deq_size(deq, proc->queue_count, proc->queue_indices) > 0 ||
           atomic_load(&proc->is_processing))
    {
        dvz_sleep(1);
    }
    log_trace("finished waiting for empty queues");
}



void dvz_deq_destroy(DvzDeq* deq)
{
    ASSERT(deq != NULL);

    for (uint32_t i = 0; i < deq->queue_count; i++)
        dvz_fifo_destroy(&deq->queues[i]);

    for (uint32_t i = 0; i < deq->proc_count; i++)
    {
        pthread_mutex_destroy(&deq->procs[i].lock);
        pthread_cond_destroy(&deq->procs[i].cond);
    }
}
