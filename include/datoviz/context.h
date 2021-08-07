/*************************************************************************************************/
/*  GPU context holding buffers and textures in video memory                                     */
/*************************************************************************************************/

#ifndef DVZ_CONTEXT_HEADER
#define DVZ_CONTEXT_HEADER

#include "colormaps.h"
#include "common.h"
#include "fifo.h"
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



// Filter type.
typedef enum
{
    DVZ_FILTER_MIN,
    DVZ_FILTER_MAG,
} DvzFilterType;



// // Dat type.
// typedef enum
// {
//     DVZ_DAT_TYPE_NONE,
//     DVZ_DAT_TYPE_STAGING,
//     DVZ_DAT_TYPE_VERTEX,
//     DVZ_DAT_TYPE_UNIFORM,
//     DVZ_DAT_TYPE_STORAGE,
// } DvzDatType;



// Dat flags.
typedef enum
{
    DVZ_DAT_FLAGS_SHARED = 0x00,     // by default, the Dat is allocated from the big buffer
    DVZ_DAT_FLAGS_STANDALONE = 0x01, // standalone DvzBuffer

    // the following are unused, may be removed
    DVZ_DAT_FLAGS_DYNAMIC = 0x10,   // will change often
    DVZ_DAT_FLAGS_RESIZABLE = 0x20, // can be resized
} DvzDatFlags;



// Tex dims.
typedef enum
{
    DVZ_TEX_NONE,
    DVZ_TEX_1D,
    DVZ_TEX_2D,
    DVZ_TEX_3D,
} DvzTexDims;



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct DvzFontAtlas DvzFontAtlas;
typedef struct DvzColorTexture DvzColorTexture;
typedef struct DvzDat DvzDat;
typedef struct DvzTex DvzTex;



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct DvzFontAtlas
{
    const char* name;
    uint32_t width, height;
    uint32_t cols, rows;
    uint8_t* font_texture;
    float glyph_width, glyph_height;
    const char* font_str;
    DvzTexture* texture;
};



struct DvzColorTexture
{
    unsigned char* arr;
    DvzTexture* texture;
};



struct DvzDat
{
    DvzObject obj;
    DvzContext* context;

    // DvzDatType type;
    int flags;
    DvzBufferRegions br;
};



struct DvzTex
{
    DvzObject obj;
    DvzContext* context;
    DvzImages* images;
    DvzTexDims dims;
    int flags;
};



struct DvzContext
{
    DvzObject obj;
    DvzGpu* gpu;

    DvzContainer buffers;
    DvzContainer dats;

    DvzContainer images;
    DvzContainer textures; // TODO: rename to texs?
    DvzContainer samplers;

    DvzContainer computes;

    // Data transfers.
    DvzDeq deq;
    DvzThread thread; // transfer thread

    // Font atlas.
    DvzFontAtlas font_atlas;
    DvzColorTexture color_texture;
    DvzTexture* transfer_texture; // Default linear 1D texture
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

/**
 * Update the colormap texture on the GPU after it has changed on the CPU.
 *
 * @param context the context
 */
DVZ_EXPORT void dvz_context_colormap(DvzContext* context);



/*************************************************************************************************/
/*  Dats and texs                                                                                */
/*************************************************************************************************/

DVZ_EXPORT DvzDat*
dvz_dat(DvzContext* ctx, DvzBufferType type, VkDeviceSize size, uint32_t count, int flags);
// choose an existing DvzBuffer, or create a new one
// allocate a buffer region

DVZ_EXPORT void dvz_dat_upload(DvzDat* dat, VkDeviceSize offset, VkDeviceSize size, void* data);
// asynchronous function
// if staging
//     allocate staging buffer if there isn't already one
// enqueue a buffer upload transfer
// the copy to staging will be done in a background thread automatically
// need the caller to call dvz_ctx_frame()
//     dequeue all pending copies, with hard gpu sync

DVZ_EXPORT void dvz_dat_download(DvzDat* dat, VkDeviceSize size, void* data);
// asynchronous function

DVZ_EXPORT void dvz_dat_destroy(DvzDat* dat);
// free the region in the buffer



DVZ_EXPORT DvzTex* dvz_tex(DvzContext* ctx, DvzTexDims dims, uvec3 shape, int flags);
// create a new image

DVZ_EXPORT void
dvz_tex_upload(DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data);

DVZ_EXPORT void
dvz_tex_download(DvzTex* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data);

DVZ_EXPORT void dvz_tex_destroy(DvzTex* tex);

DVZ_EXPORT void dvz_ctx_clear(DvzContext* ctx);
// free all buffers, delete all images



/*************************************************************************************************/
/*  Buffer allocation                                                                            */
/*************************************************************************************************/

/**
 * Allocate one of several buffer regions on the GPU.
 *
 * @param context the context
 * @param buffer_type the type of buffer to allocate the regions on
 * @param buffer_count the number of buffer regions to allocate
 * @param size the size of each region to allocate, in bytes
 */
DVZ_EXPORT DvzBufferRegions dvz_ctx_buffers(
    DvzContext* context, DvzBufferType buffer_type, uint32_t buffer_count, VkDeviceSize size);

/**
 * Resize a set of buffer regions.
 *
 * @param context the context
 * @param br the buffer regions to resize
 * @param new_size the new size of each buffer region, in bytes
 */
DVZ_EXPORT void
dvz_ctx_buffers_resize(DvzContext* context, DvzBufferRegions* br, VkDeviceSize new_size);



/*************************************************************************************************/
/*  Compute                                                                                      */
/*************************************************************************************************/

/**
 * Create a new compute pipeline.
 *
 * @param context the context
 * @param shader_path path to the `.spirv` file containing the compute shader
 */
DVZ_EXPORT DvzCompute* dvz_ctx_compute(DvzContext* context, const char* shader_path);



/*************************************************************************************************/
/*  Texture                                                                                      */
/*************************************************************************************************/

/**
 * Create a new GPU texture.
 *
 * @param context the context
 * @param dims the number of dimensions of the texture (1, 2, or 3)
 * @param size the width, height, and depth
 * @param format the format of each pixel
 */
DVZ_EXPORT DvzTexture*
dvz_ctx_texture(DvzContext* context, uint32_t dims, uvec3 size, VkFormat format);

/**
 * Resize a texture.
 *
 * !!! warning
 *     This function will delete the texture data.
 *
 * @param texture the texture
 * @param size the new size (width, height, depth)
 */
DVZ_EXPORT void dvz_texture_resize(DvzTexture* texture, uvec3 size);

/**
 * Set the texture filter.
 *
 * @param texture the texture
 * @param type the filter type
 * @param filter the filter
 */
DVZ_EXPORT void dvz_texture_filter(DvzTexture* texture, DvzFilterType type, VkFilter filter);

/**
 * Set the texture address mode.
 *
 * @param texture the texture
 * @param axis the axis
 * @param address_mode the address mode
 */
DVZ_EXPORT void dvz_texture_address_mode(
    DvzTexture* texture, DvzTextureAxis axis, VkSamplerAddressMode address_mode);

/**
 * Copy part of a texture to another texture.
 *
 * This function does not involve CPU-GPU data transfers.
 *
 * @param src the source texture
 * @param src_offset offset within the source texture
 * @param dst the target texture
 * @param dst_offset offset within the target texture
 * @param shape shape of the part of the texture to copy
 */
DVZ_EXPORT void dvz_texture_copy(
    DvzTexture* src, uvec3 src_offset, DvzTexture* dst, uvec3 dst_offset, uvec3 shape);

DVZ_EXPORT void dvz_texture_copy_from_buffer(
    DvzTexture* tex, uvec3 tex_offset, uvec3 shape, //
    DvzBufferRegions br, VkDeviceSize buf_offset, VkDeviceSize size);

DVZ_EXPORT void dvz_texture_copy_to_buffer(
    DvzTexture* tex, uvec3 tex_offset, uvec3 shape, //
    DvzBufferRegions br, VkDeviceSize buf_offset, VkDeviceSize size);

/**
 * Transition a texture to its layout.
 *
 * @param texture the texture to transition
 */
DVZ_EXPORT void dvz_texture_transition(DvzTexture* tex);

/**
 * Destroy a texture.
 *
 * @param texture the texture
 */
DVZ_EXPORT void dvz_texture_destroy(DvzTexture* texture);



#ifdef __cplusplus
}
#endif

#endif
