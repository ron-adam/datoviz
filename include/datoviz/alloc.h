/*************************************************************************************************/
/*  Allocation algorithm                                                                         */
/*************************************************************************************************/

#ifndef DVZ_ALLOC_HEADER
#define DVZ_ALLOC_HEADER

#include "array.h"
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_ALLOC_DEFAULT_COUNT 16



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct DvzAlloc DvzAlloc;
typedef struct DvzAllocSlot DvzAllocSlot;



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct DvzAllocSlot
{
    VkDeviceSize offset;
    bool occupied;
};



struct DvzAlloc
{
    uint32_t alignment; // alignment
    DvzArray items;     // each item is a pair (offset, occupied)
    uint32_t size;      // total size

    // uint32_t count;     // number of allocations
    // uint32_t arr_count; // number of items in offsets and avail
    // uint32_t* offsets;  // offset of each allocation, the size goes up to the next offset
    // bool* occupied;     // whether a given allocation is occupied or free
};



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

static inline uint32_t _align(uint32_t size, uint32_t alignment)
{
    if (alignment == 0)
        return size;
    ASSERT(alignment > 0);
    if (size % alignment == 0)
        return size;
    ASSERT(size % alignment < alignment);
    size += (alignment - (size % alignment));
    ASSERT(size % alignment == 0);
    return size;
}



static inline DvzAllocSlot* _get_slot(DvzAlloc* alloc, VkDeviceSize offset)
{
    ASSERT(alloc != NULL);
    DvzAllocSlot* slot = NULL;
    for (uint32_t i = 0; i < alloc->items.item_count; i++)
    {
        slot = (DvzAllocSlot*)dvz_array_item(&alloc->items, i);
        if (slot->offset == offset)
            return slot;
    }
    log_error("slot with offset %d not found in alloc", offset);
    return NULL;
}



static inline uint32_t _slot_idx(DvzAlloc* alloc, DvzAllocSlot* slot)
{
    ASSERT(alloc != NULL);
    if (slot == NULL)
        return 0;
    return (uint32_t)(((uint64_t)slot - (uint64_t)alloc->items.data) / alloc->items.item_size);
}



static inline VkDeviceSize _slot_size(DvzAlloc* alloc, DvzAllocSlot* slot)
{
    ASSERT(alloc != NULL);
    uint32_t idx = _slot_idx(alloc, slot);
    VkDeviceSize size = 0;
    VkDeviceSize offset = 0;
    bool occupied = slot->occupied;
    DvzAllocSlot* cur = NULL;

    for (uint32_t i = idx + 1; i < alloc->items.item_count; i++)
    {
        cur = (DvzAllocSlot*)dvz_array_item(&alloc->items, i);
        offset = cur->offset;
        if (cur->occupied != occupied)
            break;
    }
    if (offset == 0)
        offset = alloc->size;
    ASSERT(offset >= slot->offset);
    size = offset - slot->offset;
    ASSERT(size > 0);
    return size;
}



static inline bool _is_slot_available(DvzAlloc* alloc, DvzAllocSlot* slot, VkDeviceSize req_size)
{
    ASSERT(alloc != NULL);
    ASSERT(req_size > 0);
    return !slot->occupied && _slot_size(alloc, slot) >= req_size;
}



static inline void _change_slot(DvzAllocSlot* slot, bool occupied)
{
    ASSERT(slot != NULL);
    slot->occupied = occupied;
}



static inline DvzAllocSlot* _find_slot_available(DvzAlloc* alloc, VkDeviceSize req_size)
{
    ASSERT(alloc != NULL);
    DvzAllocSlot* slot = NULL;
    uint32_t i = 0;
    for (i = 0; i < alloc->items.item_count; i++)
    {
        slot = (DvzAllocSlot*)dvz_array_item(&alloc->items, i);
        ASSERT(slot != NULL);
        if (_is_slot_available(alloc, slot, req_size))
        {
            ASSERT(!slot->occupied);
            return slot;
        }
    }
    return NULL;
}



static inline void
_insert_slot_after(DvzAlloc* alloc, DvzAllocSlot* slot, VkDeviceSize offset, bool occupied)
{
    ASSERT(alloc != NULL);
    uint32_t idx = _slot_idx(alloc, slot);
    DvzAllocSlot new_slot = {.occupied = occupied, .offset = offset};
    dvz_array_insert(&alloc->items, idx + 1, 1, &new_slot);
}



static inline DvzAllocSlot* _last_slot(DvzAlloc* alloc)
{
    ASSERT(alloc != NULL);
    uint32_t idx = alloc->items.item_count - 1;
    return (DvzAllocSlot*)dvz_array_item(&alloc->items, idx);
}



static inline DvzAllocSlot* _next_slot(DvzAlloc* alloc, DvzAllocSlot* slot)
{
    ASSERT(alloc != NULL);
    ASSERT(slot != NULL);
    uint32_t idx = _slot_idx(alloc, slot);
    if (idx == alloc->items.item_count - 1)
        return NULL;
    else
        return (DvzAllocSlot*)dvz_array_item(&alloc->items, idx + 1);
}



static inline void _double_alloc_size(DvzAlloc* alloc)
{
    ASSERT(alloc != NULL);
    DvzAllocSlot* slot = _last_slot(alloc);
    ASSERT(slot != NULL);
    // If the last slot was occupied, append an available slot at the end for the newly-created
    // space.
    if (slot->occupied)
        _insert_slot_after(alloc, slot, alloc->size, false);
    alloc->size *= 2;
}



/*************************************************************************************************/
/*  Functions */
/*************************************************************************************************/

// TODO: docstrings

DVZ_INLINE DvzAlloc dvz_alloc(uint32_t size, uint32_t alignment)
{
    DvzAlloc alloc = {0};
    alloc.size = size;
    alloc.alignment = alignment;
    alloc.items = dvz_array_struct(1, sizeof(DvzAllocSlot));

    // Initially, the entire space is available.
    DvzAllocSlot* slot = (DvzAllocSlot*)dvz_array_item(&alloc.items, 0);
    slot->offset = 0;
    slot->occupied = false;
    // alloc.count = 1;
    // alloc.arr_count = DVZ_ALLOC_DEFAULT_COUNT;
    // alloc.offsets = (uint32_t*)calloc(alloc.arr_count, sizeof(uint32_t));
    // alloc.occupied = (bool*)calloc(alloc.arr_count, sizeof(bool));
    // alloc.offsets[0] = 0;
    // alloc.occupied[0] = false;
    return alloc;
}



// Return the offset of the allocation, and modify resized if needed with the new total size.
DVZ_INLINE VkDeviceSize dvz_alloc_new(DvzAlloc* alloc, uint32_t req_size, uint32_t* resized)
{
    ASSERT(alloc != NULL);

    // Try to find a slot available.
    DvzAllocSlot* slot = _find_slot_available(alloc, req_size);

    // If the found slot is available for the requested size, continue directly. Otherwise,
    // increase the alloc until it is large enough. The available slot will be the last one.
    while (slot == NULL || !_is_slot_available(alloc, slot, req_size))
    {
        _double_alloc_size(alloc);
        slot = _last_slot(alloc);
        *resized = alloc->size;
    }

    // Available slot found, possibly after resize.
    ASSERT(!slot->occupied);

    // Current slot is now occupied.
    _change_slot(slot, true);

    // We need to append a new empty slot if the new slot doesn't have exactly the right size.
    VkDeviceSize size = _slot_size(alloc, slot);
    ASSERT(size >= req_size);
    if (size > req_size)
    {
        // We need to append a new empty slot after the current one.
        _insert_slot_after(alloc, slot, req_size, false);
    }

    return slot->offset;

    // DvzAllocSlot* next = _next_slot(slot);


    // uint32_t offset, size = 0;
    // uint32_t align = alloc->alignment;
    // bool avail = false;
    // uint32_t res = 0;
    // uint32_t last_offset = 0;
    // uint32_t i = 0;
    // bool found = false;

    // // Search the first available slot that is large enough to contain the requested data.
    // for (i = 0; i < alloc->count; i++)
    // {
    //     offset = alloc->offsets[i]; // offset of the current slot
    //     ASSERT(offset % align == 0);
    //     // size of the current slot depends on the next offset
    //     size = (i < alloc->count - 1 ? alloc->offsets[i + 1] : alloc->size) - offset;
    //     // Is the current slot available?
    //     avail = !alloc->occupied[i];
    //     // Is the current slot available and large enough?
    //     if (size >= req_size && avail)
    //     {
    //         // We found a slot for our allocation!
    //         found = true;
    //         break;
    //     }
    // }

    // // No slot found?
    // if (!found)
    // {
    //     // Keep track of the last offset, in the case we need to resize.
    //     last_offset = alloc->offsets[alloc->count - 1] + avail ? 0 : size;
    //     // The last offset is either the last slot's offset if that slot was available, or the
    //     // total used size.

    //     // We'll need to resize, but we already know the offset of the new slot. It's the
    //     // aligned position just after the end of the last allocation.
    //     offset = _align(last_offset, align);
    //     // The index of the new slot depends on whether the last slot was occupied or not.
    //     i = avail ? alloc->count - 1 : alloc->count;
    // }

    // // Here, the following variables are set:
    // // - i          index of the new slot
    // // - offset     offset of the new slot
    // // - size       size of the new slot
    // ASSERT(offset % align == 0);
    // ASSERT(size % align == 0);

    // // No slot? need to resize.
    // while (!avail)
    // {
    //     size = 2 * alloc->size - offset; // new slot size
    //     alloc->size *= 2;
    //     res = alloc->size;        // signal to the caller that the allocation must be enlarged
    //     avail = size >= req_size; // the loop will end when the new size is high enough
    // }

    // ASSERT(alloc->size > 0);
    // ASSERT(size > 0);
    // ASSERT(offset + size <= alloc->size);
    // ASSERT(avail);
    // ASSERT(size >= req_size);
    // ASSERT(i < alloc->count);

    // // We need to ensure that we can at least add 2 new elements to offsets and occupied.
    // // We may need to increase the size of the arrays containing the allocations.
    // if (alloc->count + 2 > alloc->arr_count)
    // {
    //     // HACK
    //     void *_off = alloc->offsets, *_av = alloc->occupied;
    //     alloc->arr_count *= 2;
    //     REALLOC(_off, alloc->arr_count);
    //     REALLOC(_av, alloc->arr_count);
    //     alloc->offsets = (uint32_t*)_off;
    //     alloc->occupied = (bool*)_av;
    // }

    // // Ensure we have enough room in offsets and avail to add a new element.
    // ASSERT(alloc->count + 2 <= alloc->arr_count);

    // ASSERT(!alloc->occupied[i]); // the existing slot should be available

    // // Append
    // alloc->occupied[i] = true;
    // alloc->offsets[i] = offset;
    // alloc->count++;

    // return res;
}



DVZ_INLINE void dvz_alloc_free(DvzAlloc* alloc, uint32_t offset)
{
    ASSERT(alloc != NULL);
    DvzAllocSlot* slot = _get_slot(alloc, offset);
    ASSERT(slot != NULL);
    _change_slot(slot, false);
}



DVZ_INLINE void dvz_alloc_clear(DvzAlloc* alloc)
{
    ASSERT(alloc != NULL);
    alloc->size = 0;
    alloc->items.item_count = 0;
}



DVZ_INLINE void dvz_alloc_destroy(DvzAlloc* alloc)
{
    ASSERT(alloc != NULL);
    dvz_array_destroy(&alloc->items);
}



#ifdef __cplusplus
}
#endif

#endif
