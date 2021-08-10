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

/**
 * Specify the number of swapchain images.
 *
 * The Context needs this information to handle dup transfers (with one buffer region per swapchain
 * image).
 *
 * @param ctx the context
 * @param img_count the number of swapchain images
 */
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

/**
 * Allocate a new Dat.
 *
 * A Dat represents an area of data on GPU memory. It abstracts away the underlying implementation
 * based on DvzBufferRegions and DvzBuffer. The context manages the automatic allocation of Dats.
 * When Dats are freed, the space is reusable for future allocations.
 *
 * A Dat is associated to a buffer region on a buffer of the requested buffer type. The buffer may
 * be a standalone buffer containing only the Dat, or shared with other Dats, depending on the
 * flags.
 *
 * The flags also describe how data is to be transferred to the dat. There are several options
 * depending on the expected frequency of transfers and how the data is going to be used on the
 * GPU.
 *
 * Defragmentation is not implemented yet.
 *
 * @param ctx the context
 * @param type the buffer type
 * @param size the buffer size
 * @param flags the flags
 * @returns the newly-allocated Dat
 */
DVZ_EXPORT DvzDat* dvz_dat(DvzContext* ctx, DvzBufferType type, VkDeviceSize size, int flags);

/**
 * Upload data to a Dat.
 *
 * This function may be asynchronous (wait=false) or synchronous (wait=true). If it is
 * asynchronous, the function `dvz_transfers_frame()` must be called at every frame in the event
 * loop.
 *
 * This function handles all types of uploads: with or without a staging buffer, normal or dup
 * transfers, etc.
 *
 * @param dat the Dat
 * @param offset the offset within the Dat
 * @param size the size of the data to upload to the Dat
 * @param wait whether this function should wait until the upload is complete or not
 */
DVZ_EXPORT void
dvz_dat_upload(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data, bool wait);

/**
 * Download data from a Dat.
 *
 * @param dat the Dat
 * @param offset the offset within the Dat
 * @param size the size of the data to download from the Dat
 * @param wait whether this function should wait until the download is complete or not
 */
DVZ_EXPORT void
dvz_dat_download(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data, bool wait);

/**
 * Resize a dat.
 *
 * !!! note
 *     Not implemented yet: deciding whether the existing data should be kept or not upon resizing.
 *
 * @param dat the Dat
 * @param new_size the new size
 */
DVZ_EXPORT void dvz_dat_resize(DvzDat* dat, VkDeviceSize new_size);



/*************************************************************************************************/
/*  Texs                                                                                         */
/*************************************************************************************************/

/**
 * Create a new Tex.
 *
 * A Tex represents an image on the GPU. It abstracts away the underlying GPU implementation based
 * on DvzImages, itself based on VkImage.
 *
 * @param ctx the context
 * @param dims the number of dimensions of the image (1D, 2D, or 3D)
 * @param shape the width, height, depth
 * @param format the image format
 * @param flags the flags
 * @returns the Tex
 */
DVZ_EXPORT DvzTex*
dvz_tex(DvzContext* ctx, DvzTexDims dims, uvec3 shape, VkFormat format, int flags);

/**
 * Upload data to a Tex.
 *
 * @param tex the Tex
 * @param offset the offset within the image
 * @param shape the width, height, depth of the data to upload
 * @param size the number of bytes of the data
 * @param data the data
 * @param wait whether this function should wait until the upload is complete or not
 */
DVZ_EXPORT void
dvz_tex_upload(DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, bool wait);

/**
 * Download data from a Tex.
 *
 * @param tex the Tex
 * @param offset the offset within the image
 * @param shape the width, height, depth of the data to download
 * @param size the number of bytes of the data
 * @param data the data
 * @param wait whether this function should wait until the download is complete or not
 */
DVZ_EXPORT void
dvz_tex_download(DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data, bool wait);

/**
 * Resize a Tex.
 *
 * @param tex the Tex
 * @param new_shape the new width, height, depth
 * @param new_size the number of bytes corresponding to the new image shape
 */
DVZ_EXPORT void dvz_tex_resize(DvzTex* tex, uvec3 new_shape, VkDeviceSize new_size);



#ifdef __cplusplus
}
#endif

#endif
