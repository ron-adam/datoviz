/*************************************************************************************************/
/*  Make GPU data allocation                                                                     */
/*************************************************************************************************/

#ifndef DVZ_ALLOCS_UTILS_HEADER
#define DVZ_ALLOCS_UTILS_HEADER

#include "../include/datoviz/allocs.h"
#include "../include/datoviz/resources.h"
#include "resources_utils.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Allocs utils                                                                                 */
/*************************************************************************************************/

static VkDeviceSize _find_alignment(DvzAllocs* allocs, DvzBufferType type)
{
    ASSERT(allocs != NULL);
    ASSERT(type < DVZ_BUFFER_TYPE_COUNT);

    VkDeviceSize alignment = 0;
    bool needs_align = type == DVZ_BUFFER_TYPE_UNIFORM || type == DVZ_BUFFER_TYPE_MAPPABLE;
    if (needs_align)
        alignment = allocs->gpu->device_properties.limits.minUniformBufferOffsetAlignment;
    return alignment;
}



static DvzAlloc* _get_alloc(DvzAllocs* allocs, DvzBufferType type)
{
    ASSERT(allocs != NULL);
    ASSERT((uint32_t)type < DVZ_BUFFER_TYPE_COUNT);
    return &allocs->allocators[(uint32_t)type];
}



static DvzAlloc* _make_allocator(DvzAllocs* allocs, DvzBufferType type, VkDeviceSize size)
{
    ASSERT(allocs != NULL);
    ASSERT((uint32_t)type < DVZ_BUFFER_TYPE_COUNT);

    DvzAlloc* alloc = _get_alloc(allocs, type);
    VkDeviceSize alignment = _find_alignment(allocs, type);
    *alloc = dvz_alloc(size, alignment);
    return alloc;
}



static VkDeviceSize
_allocate_dat(DvzAllocs* allocs, DvzResources* res, DvzBufferType type, VkDeviceSize req_size)
{
    ASSERT(allocs != NULL);
    ASSERT(type < DVZ_BUFFER_TYPE_COUNT);
    ASSERT(req_size > 0);

    VkDeviceSize resized = 0; // will be non-zero if the buffer must be resized
    DvzAlloc* alloc = _get_alloc(allocs, type);
    // Make the allocation.
    VkDeviceSize offset = dvz_alloc_new(alloc, req_size, &resized);

    // Need to resize the underlying DvzBuffer.
    if (resized)
    {
        DvzBuffer* buffer = _get_shared_buffer(res, type);
        log_info("reallocating buffer %d to %s", type, pretty_size(resized));
        dvz_buffer_resize(buffer, resized);
    }

    return offset;
}



#ifdef __cplusplus
}
#endif

#endif
