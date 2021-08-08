#include "../include/datoviz/allocs.h"
#include "allocs_utils.h"
#include <stdlib.h>



/*************************************************************************************************/
/*  Allocs                                                                                       */
/*************************************************************************************************/

void dvz_allocs(DvzGpu* gpu, DvzResources* res, DvzAllocs* allocs)
{
    ASSERT(gpu != NULL);
    ASSERT(dvz_obj_is_created(&gpu->obj));
    ASSERT(allocs != NULL);
    ASSERT(!dvz_obj_is_created(&allocs->obj));
    // NOTE: this function should only be called once, at context creation.

    log_trace("creating allocs");

    // Create the resources.
    allocs->gpu = gpu;

    _make_allocator(allocs, res, DVZ_BUFFER_TYPE_STAGING, DVZ_BUFFER_TYPE_STAGING_SIZE);
    _make_allocator(allocs, res, DVZ_BUFFER_TYPE_VERTEX, DVZ_BUFFER_TYPE_VERTEX_SIZE);
    _make_allocator(allocs, res, DVZ_BUFFER_TYPE_INDEX, DVZ_BUFFER_TYPE_INDEX_SIZE);
    _make_allocator(allocs, res, DVZ_BUFFER_TYPE_STORAGE, DVZ_BUFFER_TYPE_STORAGE_SIZE);
    _make_allocator(allocs, res, DVZ_BUFFER_TYPE_UNIFORM, DVZ_BUFFER_TYPE_UNIFORM_SIZE);
    _make_allocator(allocs, res, DVZ_BUFFER_TYPE_MAPPABLE, DVZ_BUFFER_TYPE_MAPPABLE_SIZE);

    dvz_obj_created(&allocs->obj);
}



void dvz_allocs_destroy(DvzAllocs* allocs)
{
    if (allocs == NULL)
    {
        log_error("skip destruction of null allocs");
        return;
    }
    log_trace("destroying allocs");
    ASSERT(allocs != NULL);
    ASSERT(allocs->gpu != NULL);

    // Destroy the DvzDat allocators.
    for (uint32_t i = 0; i < DVZ_BUFFER_TYPE_COUNT; i++)
        dvz_alloc_destroy(&allocs->allocators[i]);

    dvz_obj_destroyed(&allocs->obj);
}
