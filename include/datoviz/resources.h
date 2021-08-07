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



// Filter type.
typedef enum
{
    DVZ_FILTER_MIN,
    DVZ_FILTER_MAG,
} DvzFilterType;



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct DvzDat DvzDat;
typedef struct DvzTex DvzTex;

typedef struct DvzFontAtlas DvzFontAtlas;
typedef struct DvzColorTexture DvzColorTexture;

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
/*  Common resources                                                                             */
/*************************************************************************************************/

struct DvzFontAtlas
{
    const char* name;
    uint32_t width, height;
    uint32_t cols, rows;
    uint8_t* font_texture;
    float glyph_width, glyph_height;
    const char* font_str;

    // TODO
    // DvzTexture* texture;
};



struct DvzColorTexture
{
    unsigned char* arr;

    // TODO
    // DvzTexture* texture;
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
    DvzContainer textures; // TODO: rename to texs?
    DvzContainer samplers;
    DvzContainer computes;

    // Common resources.
    DvzFontAtlas font_atlas;
    DvzColorTexture color_texture;
    DvzTexture* transfer_texture; // Default linear 1D texture
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

/**
 * Destroy a resources object.
 *
 * @param res the DvzResources pointer
 */
DVZ_EXPORT void dvz_resources_destroy(DvzResources* res);



#ifdef __cplusplus
}
#endif

#endif
