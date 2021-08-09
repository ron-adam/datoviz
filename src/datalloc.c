#include "../include/datoviz/datalloc.h"
#include "datalloc_utils.h"
#include <stdlib.h>



/*************************************************************************************************/
/*  Allocs                                                                                       */
/*************************************************************************************************/

void dvz_datalloc(DvzGpu* gpu, DvzResources* res, DvzDatAlloc* datalloc)
{
    ASSERT(gpu != NULL);
    ASSERT(dvz_obj_is_created(&gpu->obj));
    ASSERT(datalloc != NULL);
    ASSERT(!dvz_obj_is_created(&datalloc->obj));
    // NOTE: this function should only be called once, at context creation.

    log_trace("creating datalloc");

    // Create the resources.
    datalloc->gpu = gpu;

    // Initialize the allocators for all possible types of shared buffers.
    for (uint32_t i = 0; i < DVZ_BUFFER_TYPE_COUNT; i++)
    {
        _make_allocator(datalloc, res, (DvzBufferType)i, false, DVZ_BUFFER_DEFAULT_SIZE);
        _make_allocator(datalloc, res, (DvzBufferType)i, true, DVZ_BUFFER_DEFAULT_SIZE);
    }

    dvz_obj_created(&datalloc->obj);
}



void dvz_datalloc_destroy(DvzDatAlloc* datalloc)
{
    if (datalloc == NULL)
    {
        log_error("skip destruction of null datalloc");
        return;
    }
    log_trace("destroying datalloc");
    ASSERT(datalloc != NULL);
    ASSERT(datalloc->gpu != NULL);

    // Destroy the DvzDat allocators.
    for (uint32_t i = 0; i < DVZ_BUFFER_TYPE_COUNT; i++)
        dvz_alloc_destroy(&datalloc->allocators[i]);

    dvz_obj_destroyed(&datalloc->obj);
}
