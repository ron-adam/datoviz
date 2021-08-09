/*************************************************************************************************/
/*  Holds all GPU data resources (buffers, images, dats, texs)                                   */
/*************************************************************************************************/

#ifndef DVZ_RESOURCES_HEADER
#define DVZ_RESOURCES_HEADER

#include "common.h"
#include "vklite.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define TRANSFERABLE (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)

#define DVZ_BUFFER_DEFAULT_SIZE (1 * 1024 * 1024)
// #define DVZ_BUFFER_TYPE_STAGING_SIZE (4 * 1024 * 1024)
// #define DVZ_BUFFER_TYPE_VERTEX_SIZE  (4 * 1024 * 1024)
// #define DVZ_BUFFER_TYPE_INDEX_SIZE   (4 * 1024 * 1024)
// #define DVZ_BUFFER_TYPE_STORAGE_SIZE (1 * 1024 * 1024)
// #define DVZ_BUFFER_TYPE_UNIFORM_SIZE (1 * 1024 * 1024)
// #define DVZ_BUFFER_TYPE_MAPPABLE_SIZE DVZ_BUFFER_TYPE_UNIFORM_SIZE



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// // TODO: remove?
// // Dat flags.
// typedef enum
// {
//     DVZ_DAT_FLAGS_SHARED = 0x01,     // by default, the Dat is allocated from the big buffer
//     DVZ_DAT_FLAGS_STANDALONE = 0x02, // standalone DvzBuffer

//     // the following are unused, may be removed
//     DVZ_DAT_FLAGS_DYNAMIC = 0x10,   // will change often
//     DVZ_DAT_FLAGS_RESIZABLE = 0x20, // can be resized
// } DvzDatFlags;



// Dat usage.
// TODO: not implemented yet, going from these flags to DvzDatOptions
typedef enum
{
    DVZ_DAT_USAGE_FREQUENT_NONE,
    DVZ_DAT_USAGE_FREQUENT_UPLOAD = 0x0001,
    DVZ_DAT_USAGE_FREQUENT_DOWNLOAD = 0x0002,
    DVZ_DAT_USAGE_FREQUENT_RESIZE = 0x0004,
} DvzDatUsage;



// Dat options.
typedef enum
{
    DVZ_DAT_OPTIONS_NONE = 0x0000,               // default: shared, with staging, single copy
    DVZ_DAT_OPTIONS_STANDALONE = 0x0100,         // (or shared)
    DVZ_DAT_OPTIONS_MAPPABLE = 0x0200,           // (or non-mappable = need staging buffer)
    DVZ_DAT_OPTIONS_DUP = 0x0400,                // (or single copy)
    DVZ_DAT_OPTIONS_KEEP_ON_RESIZE = 0x1000,     // (or loose the data when resizing the buffer)
    DVZ_DAT_OPTIONS_PERSISTENT_STAGING = 0x2000, // (or recreate the staging buffer every time)
} DvzDatOptions;



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

typedef struct DvzDat DvzDat;
typedef struct DvzTex DvzTex;
typedef struct DvzResources DvzResources;



/*************************************************************************************************/
/*  Dat and Tex                                                                                  */
/*************************************************************************************************/

struct DvzDat
{
    DvzObject obj;
    DvzContext* context;

    int flags;
    DvzBufferRegions br;

    DvzDat* stg; // used for persistent staging, resized when the dat is resized
};



struct DvzTex
{
    DvzObject obj;
    DvzContext* context;
    DvzImages* images;
    DvzTexDims dims;
    int flags;
};



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

struct DvzResources
{
    DvzObject obj;
    DvzGpu* gpu;

    DvzContainer buffers;
    DvzContainer images;
    DvzContainer dats;
    DvzContainer texs;
    DvzContainer samplers;
    DvzContainer computes;
};



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

/**
 * Create a resources object.
 *
 * @param gpu the GPU
 * @param res the DvzResources pointer
 */
DVZ_EXPORT void dvz_resources(DvzGpu* gpu, DvzResources* res);

DVZ_EXPORT DvzBuffer*
dvz_resources_buffer(DvzResources* res, DvzBufferType type, bool mappable, VkDeviceSize size);

DVZ_EXPORT DvzImages*
dvz_resources_image(DvzResources* res, DvzTexDims dims, uvec3 shape, VkFormat format);

DVZ_EXPORT DvzSampler*
dvz_resources_sampler(DvzResources* res, VkFilter filter, VkSamplerAddressMode mode);

DVZ_EXPORT DvzCompute* dvz_resources_compute(DvzResources* res, const char* shader_path);

/**
 * Destroy a resources object.
 *
 * @param res the DvzResources pointer
 */
DVZ_EXPORT void dvz_resources_destroy(DvzResources* res);



DVZ_EXPORT void dvz_dat_destroy(DvzDat* dat);

DVZ_EXPORT void dvz_tex_destroy(DvzTex* tex);



#ifdef __cplusplus
}
#endif

#endif
