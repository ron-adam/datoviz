/*************************************************************************************************/
/*  GPU context holding buffers and textures in video memory                                     */
/*************************************************************************************************/

#ifndef DVZ_CONTEXT_HEADER
#define DVZ_CONTEXT_HEADER

#include "atlases.h"
#include "colormaps.h"
#include "common.h"
#include "datalloc.h"
#include "fifo.h"
#include "resources.h"
#include "transfers.h"
#include "vklite.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_DEFAULT_WIDTH  800
#define DVZ_DEFAULT_HEIGHT 600

#define DVZ_ZERO_OFFSET                                                                           \
    (uvec3) { 0, 0, 0 }



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// Default queue.
typedef enum
{
    // NOTE: by convention in vklite, the first queue MUST support transfers
    DVZ_DEFAULT_QUEUE_TRANSFER,
    DVZ_DEFAULT_QUEUE_COMPUTE,
    DVZ_DEFAULT_QUEUE_RENDER,
    DVZ_DEFAULT_QUEUE_PRESENT,
    DVZ_DEFAULT_QUEUE_COUNT,
} DvzDefaultQueue;



struct DvzContext
{
    DvzObject obj;
    DvzGpu* gpu;
    uint32_t img_count;

    // Companion objects, all of them should be testable independently of the others and the
    // context. However, the DvzAllocs objects *depends* on the DvzResources.
    DvzTransfers transfers;
    DvzResources res;
    DvzDatAlloc datalloc;
    DvzAtlases atlases;
};



/*************************************************************************************************/
/*  Context                                                                                      */
/*************************************************************************************************/

/**
 * Create a GPU with default queues and features.
 *
 * @param gpu the GPU
 * @param window the associated window (optional)
 */
DVZ_EXPORT void dvz_gpu_default(DvzGpu* gpu, DvzWindow* window);

/**
 * Create a context associated to a GPU.
 *
 * !!! note
 *     The GPU must have been created beforehand.
 *
 * @param gpu the GPU
 */
DVZ_EXPORT DvzContext* dvz_context(DvzGpu* gpu);

DVZ_EXPORT void dvz_context_img_count(DvzContext* ctx, uint32_t img_count);

/**
 * Destroy all GPU resources in a GPU context.
 *
 * @param context the context
 */
DVZ_EXPORT void dvz_context_reset(DvzContext* context);

/**
 * Reset all GPUs.
 *
 * @param app the application instance
 */
DVZ_EXPORT void dvz_app_reset(DvzApp* app);



/*************************************************************************************************/
/*  Dats                                                                                         */
/*************************************************************************************************/

// TODO: docstrings

DVZ_EXPORT DvzDat* dvz_dat(DvzContext* ctx, DvzBufferType type, VkDeviceSize size, int flags);

// if not wait, one needs to call dvz_transfers_frame() for at least 1 frame for standard
// transfers, or for all N frames for dup transfers.
DVZ_EXPORT void
dvz_dat_upload(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data, bool wait);

DVZ_EXPORT void
dvz_dat_download(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data, bool wait);

DVZ_EXPORT void dvz_dat_resize(DvzDat* dat, VkDeviceSize new_size);



/*************************************************************************************************/
/*  Texs                                                                                         */
/*************************************************************************************************/

DVZ_EXPORT DvzTex*
dvz_tex(DvzContext* ctx, DvzTexDims dims, uvec3 shape, VkFormat format, int flags);

DVZ_EXPORT void
dvz_tex_upload(DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, bool wait);

DVZ_EXPORT void
dvz_tex_download(DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, bool wait);

DVZ_EXPORT void dvz_tex_resize(DvzTex* tex, uvec3 new_shape, VkDeviceSize new_size);



#ifdef __cplusplus
}
#endif

#endif
