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

#define DVZ_BUFFER_TYPE_STAGING_SIZE  (4 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_VERTEX_SIZE   (4 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_INDEX_SIZE    (4 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_STORAGE_SIZE  (1 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_UNIFORM_SIZE  (1 * 1024 * 1024)
#define DVZ_BUFFER_TYPE_MAPPABLE_SIZE DVZ_BUFFER_TYPE_UNIFORM_SIZE



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// Dat flags.
typedef enum
{
    DVZ_DAT_FLAGS_SHARED = 0x01,     // by default, the Dat is allocated from the big buffer
    DVZ_DAT_FLAGS_STANDALONE = 0x02, // standalone DvzBuffer

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
dvz_resources_buffer(DvzResources* res, DvzBufferType type, VkDeviceSize size);

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
