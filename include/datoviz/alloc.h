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
    VkDeviceSize alignment; // alignment, in bytes
    DvzArray items;         // each item is a pair (offset, occupied)
    VkDeviceSize size;      // total size, in bytes
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



static inline void _check_offset_alignment(DvzAlloc* alloc, VkDeviceSize offset)
{
    ASSERT(alloc != NULL);
    if (alloc->alignment > 0)
        ASSERT(offset % alloc->alignment == 0);
}



static inline DvzAllocSlot* _get_slot(DvzAlloc* alloc, VkDeviceSize offset)
{
    ASSERT(alloc != NULL);
    _check_offset_alignment(alloc, offset);
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
        _check_offset_alignment(alloc, offset);
        if (cur->occupied != occupied)
            break;
    }
    if (offset == 0)
        offset = alloc->size;
    ASSERT(offset >= slot->offset);
    _check_offset_alignment(alloc, offset);
    size = offset - slot->offset;
    ASSERT(size > 0);
    _check_offset_alignment(alloc, size);
    return size;
}



static inline bool _is_slot_available(DvzAlloc* alloc, DvzAllocSlot* slot, VkDeviceSize req_size)
{
    ASSERT(alloc != NULL);
    ASSERT(req_size > 0);
    _check_offset_alignment(alloc, req_size);
    VkDeviceSize size = _slot_size(alloc, slot);
    _check_offset_alignment(alloc, size);
    return !slot->occupied && size >= req_size;
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
    _check_offset_alignment(alloc, offset);
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
    _check_offset_alignment(alloc, alloc->size);
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

DVZ_INLINE DvzAlloc dvz_alloc(VkDeviceSize size, VkDeviceSize alignment)
{
    DvzAlloc alloc = {0};
    alloc.alignment = alignment;
    _check_offset_alignment(&alloc, size);
    alloc.size = size;
    alloc.items = dvz_array_struct(1, sizeof(DvzAllocSlot));

    // Initially, the entire space is available.
    DvzAllocSlot* slot = (DvzAllocSlot*)dvz_array_item(&alloc.items, 0);
    slot->offset = 0;
    slot->occupied = false;
    return alloc;
}



// Return the offset of the allocation, and modify resized if needed with the new total size.
DVZ_INLINE VkDeviceSize
dvz_alloc_new(DvzAlloc* alloc, VkDeviceSize req_size, VkDeviceSize* resized)
{
    ASSERT(alloc != NULL);
    req_size = _align(req_size, alloc->alignment);

    // Try to find a slot available.
    DvzAllocSlot* slot = _find_slot_available(alloc, req_size);

    // If the found slot is available for the requested size, continue directly. Otherwise,
    // increase the alloc until it is large enough. The available slot will be the last one.
    while (slot == NULL || !_is_slot_available(alloc, slot, req_size))
    {
        _double_alloc_size(alloc);
        slot = _last_slot(alloc);
        if (resized != NULL)
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
        _insert_slot_after(alloc, slot, slot->offset + req_size, false);
    }

    return slot->offset;
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
