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

static DvzAlloc** _get_alloc(DvzDatAlloc* datalloc, DvzBufferType type, bool mappable)
{
    ASSERT(datalloc != NULL);
    CHECK_BUFFER_TYPE

    uint32_t idx = 2 * (uint32_t)(type - 1) + (uint32_t)mappable - 1;
    ASSERT(idx < 2 * DVZ_BUFFER_TYPE_COUNT - 1);
    return &datalloc->allocators[idx];
}



static DvzAlloc* _make_allocator(
    DvzDatAlloc* datalloc, DvzResources* res, DvzBufferType type, bool mappable, VkDeviceSize size)
{
    ASSERT(datalloc != NULL);
    CHECK_BUFFER_TYPE

    DvzAlloc** alloc = _get_alloc(datalloc, type, mappable);

    // Find alignment by looking at the buffers themselves.

    // WARNING: currently, a side-effect of requesting a buffer just to get the alignment is that
    // all shared buffers are automatically created here.

    DvzBuffer* buffer = _get_shared_buffer(res, type, mappable);
    VkDeviceSize alignment = buffer->vma.alignment;
    ASSERT(alignment > 0);

    *alloc = dvz_alloc(size, alignment);
    return *alloc;
}



static VkDeviceSize _allocate_dat(
    DvzDatAlloc* datalloc, DvzResources* res, DvzBufferType type, bool mappable,
    VkDeviceSize req_size)
{
    ASSERT(datalloc != NULL);
    ASSERT(req_size > 0);
    CHECK_BUFFER_TYPE

    VkDeviceSize resized = 0; // will be non-zero if the buffer must be resized
    DvzAlloc** alloc = _get_alloc(datalloc, type, mappable);
    // Make the allocation.
    VkDeviceSize offset = dvz_alloc_new(*alloc, req_size, &resized);

    // Need to resize the underlying DvzBuffer.
    if (resized)
    {
        DvzBuffer* buffer = _get_shared_buffer(res, type, mappable);
        log_info("resizing buffer %d (mappable: %d) to %s", type, mappable, pretty_size(resized));
        dvz_buffer_resize(buffer, resized);
    }

    return offset;
}



static void
_deallocate_dat(DvzDatAlloc* datalloc, DvzBufferType type, bool mappable, VkDeviceSize offset)
{
    ASSERT(datalloc != NULL);
    CHECK_BUFFER_TYPE

    // Get the abstract DvzAlloc object associated to the dat's buffer.
    DvzAlloc** alloc = _get_alloc(datalloc, type, mappable);
    dvz_alloc_free(*alloc, offset);
}


#ifdef __cplusplus
}
#endif

#endif
