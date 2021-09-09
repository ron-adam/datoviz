/*************************************************************************************************/
/*  GPU data allocation                                                                          */
/*************************************************************************************************/

#ifndef DVZ_ALLOCS_HEADER
#define DVZ_ALLOCS_HEADER

#include "alloc.h"
#include "common.h"
#include "resources.h"
#include "vklite.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct DvzDatAlloc DvzDatAlloc;



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct DvzDatAlloc
{
    DvzObject obj;
    DvzGpu* gpu;

    DvzAlloc* allocators[2 * DVZ_BUFFER_TYPE_COUNT - 1]; // one dat allocator for each buffer(each
                                                         // type may be mappable or not)
};



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

/**
 * Create a datalloc object.
 *
 * This object is responsible for the allocation of buffer regions on GPU buffers. It is used by
 * the context when allocating new Dats.
 *
 * @param gpu the GPU
 * @param res the DvzResources object
 * @param datalloc the DatAlloc object
 */
DVZ_EXPORT void dvz_datalloc(DvzGpu* gpu, DvzResources* res, DvzDatAlloc* datalloc);

/**
 * Destroy a datalloc object.
 *
 * @param datalloc the datalloc
 */
DVZ_EXPORT void dvz_datalloc_destroy(DvzDatAlloc* datalloc);



#ifdef __cplusplus
}
#endif

#endif
