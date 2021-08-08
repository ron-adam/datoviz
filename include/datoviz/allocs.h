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

typedef struct DvzAllocs DvzAllocs;



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct DvzAllocs
{
    DvzObject obj;
    DvzGpu* gpu;

    DvzAlloc allocators[DVZ_BUFFER_TYPE_COUNT]; // one dat allocator for each buffer
};



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

/**
 * Create an allocs object.
 *
 * @param gpu the GPU
 * @param allocs the allocs
 */
DVZ_EXPORT void dvz_allocs(DvzGpu* gpu, DvzResources* res, DvzAllocs* allocs);

/**
 * Destroy an allocs object.
 *
 * @param allocs the allocs
 */
DVZ_EXPORT void dvz_allocs_destroy(DvzAllocs* allocs);



#ifdef __cplusplus
}
#endif

#endif
