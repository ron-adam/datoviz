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



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

static inline VkDeviceSize _align(VkDeviceSize size, VkDeviceSize alignment)
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



static inline void _check_align(VkDeviceSize n, VkDeviceSize alignment)
{
    ASSERT(alignment == 0 || n % alignment == 0);
}



/*************************************************************************************************/
/*  Functions */
/*************************************************************************************************/

/**
 * Create an abstract allocation object.
 *
 * This object handles allocation on a virtual buffer of a given size. It takes care of alignment.
 *
 * @param size the total size of the underlying buffer
 * @param alignment the required alignment for allocations
 */
DVZ_EXPORT DvzAlloc* dvz_alloc(VkDeviceSize size, VkDeviceSize alignment);

/**
 * Make a new allocation.
 *
 * @param alloc the DvzAlloc pointer
 * @param req_size the requested size for the allocation
 * @param[out] resized if the underlying virtual buffer had to be resized, the new size
 * @returns the offset of the allocated item within the virtual buffer
 */
DVZ_EXPORT VkDeviceSize
dvz_alloc_new(DvzAlloc* alloc, VkDeviceSize req_size, VkDeviceSize* resized);

/**
 * Remove an allocated item.
 *
 * @param alloc the DvzAlloc pointer
 * @param offset the offset of the allocated item to be removed
 */
DVZ_EXPORT void dvz_alloc_free(DvzAlloc* alloc, VkDeviceSize offset);

DVZ_EXPORT VkDeviceSize dvz_alloc_get(DvzAlloc* alloc, VkDeviceSize offset);

/**
 * Clear all allocations.
 *
 * @param alloc the DvzAlloc pointer
 */
DVZ_EXPORT void dvz_alloc_clear(DvzAlloc* alloc);


/**
 * Destroy a DvzAlloc instance.
 *
 * @param alloc the DvzAlloc pointer
 */
DVZ_EXPORT void dvz_alloc_destroy(DvzAlloc* alloc);



#ifdef __cplusplus
}
#endif

#endif
