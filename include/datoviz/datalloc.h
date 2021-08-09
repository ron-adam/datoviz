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

    DvzAlloc allocators[2 * DVZ_BUFFER_TYPE_COUNT - 1]; // one dat allocator for each buffer(each
                                                        // type may be mappable or not)
};



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

/**
 * Create an datalloc object.
 *
 * @param gpu the GPU
 * @param datalloc the datalloc
 */
DVZ_EXPORT void dvz_datalloc(DvzGpu* gpu, DvzResources* res, DvzDatAlloc* datalloc);

/**
 * Destroy an datalloc object.
 *
 * @param datalloc the datalloc
 */
DVZ_EXPORT void dvz_datalloc_destroy(DvzDatAlloc* datalloc);



#ifdef __cplusplus
}
#endif

#endif
