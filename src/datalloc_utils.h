/*************************************************************************************************/
/*  Make GPU data allocation                                                                     */
/*************************************************************************************************/

#ifndef DVZ_ALLOCS_UTILS_HEADER
#define DVZ_ALLOCS_UTILS_HEADER

#include "../include/datoviz/datalloc.h"
#include "../include/datoviz/resources.h"
#include "resources_utils.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Allocs utils                                                                                 */
/*************************************************************************************************/

static DvzAlloc* _get_alloc(DvzDatAlloc* datalloc, DvzBufferType type)
{
    ASSERT(datalloc != NULL);
    ASSERT((uint32_t)type < DVZ_BUFFER_TYPE_COUNT);
    return &datalloc->allocators[(uint32_t)type];
}



static DvzAlloc*
_make_allocator(DvzDatAlloc* datalloc, DvzResources* res, DvzBufferType type, VkDeviceSize size)
{
    ASSERT(datalloc != NULL);
    ASSERT((uint32_t)type < DVZ_BUFFER_TYPE_COUNT);

    DvzAlloc* alloc = _get_alloc(datalloc, type);

    // Find alignment by looking at the buffers themselves.
    DvzBuffer* buffer = (DvzBuffer*)dvz_container_get(&res->buffers, type);
    VkDeviceSize alignment = buffer->vma.alignment;
    ASSERT(alignment > 0);

    *alloc = dvz_alloc(size, alignment);
    return alloc;
}



static VkDeviceSize
_allocate_dat(DvzDatAlloc* datalloc, DvzResources* res, DvzBufferType type, VkDeviceSize req_size)
{
    ASSERT(datalloc != NULL);
    ASSERT(type < DVZ_BUFFER_TYPE_COUNT);
    ASSERT(req_size > 0);

    VkDeviceSize resized = 0; // will be non-zero if the buffer must be resized
    DvzAlloc* alloc = _get_alloc(datalloc, type);
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
