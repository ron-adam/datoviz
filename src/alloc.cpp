#include "../include/datoviz/alloc.h"
#include <list>
#include <map>


/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

extern "C" struct DvzAlloc
{
    std::map<VkDeviceSize, VkDeviceSize> occupied;
    std::map<VkDeviceSize, VkDeviceSize> free;
    VkDeviceSize alignment, alloc_size, buf_size;
};



/*************************************************************************************************/
/*  Functions                                                                                    */
/*************************************************************************************************/

DvzAlloc* dvz_alloc(VkDeviceSize size, VkDeviceSize alignment)
{
    ASSERT(size > 0);

    DvzAlloc* alloc = new DvzAlloc();
    alloc->occupied = std::map<VkDeviceSize, VkDeviceSize>();
    alloc->free = std::map<VkDeviceSize, VkDeviceSize>();

    alloc->alignment = alignment;
    alloc->buf_size = size;

    return alloc;
}



VkDeviceSize dvz_alloc_new(DvzAlloc* alloc, VkDeviceSize req_size, VkDeviceSize* resized)
{
    ASSERT(alloc != NULL);

    VkDeviceSize req = _align(req_size, alloc->alignment);
    ASSERT(req >= req_size);

    // Find a free slot large enough for the req size.
    for (const auto& [o, s] : alloc->free)
    {
        // The free slot is larger than the requested size: we can take it!
        if (req <= s)
        {
            // Make sure this slot is not already occupied.
            ASSERT(alloc->occupied.count(o) == 0);
            // Add the new allocated slot.
            alloc->occupied[o] = req;
            // If empty space remains, update the existing empty space.
            if (s > req)
            {
                alloc->free[o + req] = s - req;
            }
            // In all cases, remove the existing free slot.
            alloc->free.erase(o);
            return o;
        }
    }



    // If we're here, it means we weren't able to find a free slot. We must create a new slot after
    // all existing slots.
    _check_align(alloc->alloc_size, alloc->alignment);

    // Add a new occupied slot.
    // Ensure the new slot doesn't already exist.
    ASSERT(alloc->occupied.count(alloc->alloc_size) == 0);
    alloc->occupied[alloc->alloc_size] = req;
    VkDeviceSize out = alloc->alloc_size;
    // Increase the total allocated size.
    alloc->alloc_size += req;

    // Need to resize the underlying buffer?
    if (alloc->alloc_size > alloc->buf_size)
    {
        // Double the buffer size until it is larger or equal than the allocated size.
        // VkDeviceSize new_size = alloc->buf_size;
        while (alloc->buf_size < alloc->alloc_size)
            alloc->buf_size *= 2;
        ASSERT(alloc->alloc_size <= alloc->buf_size);
        // Change the passed pointer to let the caller know that the underlying buffer had to be
        // resized.
        if (resized != NULL)
            *resized = alloc->buf_size;
        log_trace("will need to resize alloc buffer to %s", pretty_size(alloc->buf_size));
    }

    return out;
}



VkDeviceSize dvz_alloc_get(DvzAlloc* alloc, VkDeviceSize offset)
{
    ASSERT(alloc != NULL);
    if (alloc->occupied.count(offset) > 0)
    {
        return alloc->occupied[offset];
    }
    return 0;
}



void dvz_alloc_free(DvzAlloc* alloc, VkDeviceSize offset)
{
    ASSERT(alloc != NULL);
    // Check that the slot is occupied.
    ASSERT(alloc->occupied.count(offset) > 0);
    ASSERT(alloc->free.count(offset) == 0);
    VkDeviceSize size = alloc->occupied[offset];
    ASSERT(size > 0);
    // Remove the slot and put it in the free map.
    alloc->occupied.erase(offset);
    alloc->free[offset] = size;
}



void dvz_alloc_clear(DvzAlloc* alloc)
{
    ASSERT(alloc != NULL);
    alloc->occupied.clear();
    alloc->free.clear();
    alloc->alloc_size = 0;
}



void dvz_alloc_destroy(DvzAlloc* alloc)
{
    ASSERT(alloc != NULL);
    delete alloc;
}
