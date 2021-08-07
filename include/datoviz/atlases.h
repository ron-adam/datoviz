/*************************************************************************************************/
/*  Shared GPU resources, typically atlases                                                      */
/*************************************************************************************************/

#ifndef DVZ_ATLASES_HEADER
#define DVZ_ATLASES_HEADER

#include "resources.h"



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct DvzFontAtlas DvzFontAtlas;
typedef struct DvzColormapAtlas DvzColormapAtlas;
typedef struct DvzAtlases DvzAtlases;



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct DvzFontAtlas
{
    const char* name;
    uint32_t width, height;
    uint32_t cols, rows;
    uint8_t* font_data;
    float glyph_width, glyph_height;
    const char* font_str;
    DvzImages* img;
};



struct DvzColormapAtlas
{
    unsigned char* arr;
    DvzImages* img;
};



struct DvzAtlases
{
    DvzObject obj;
    DvzContext* ctx;

    DvzFontAtlas font_atlas;
    DvzColormapAtlas cmap_atlas;
    DvzImages* transfer_atlas; // Default linear 1D texture
};



/*************************************************************************************************/
/*  Functions                                                                                    */
/*************************************************************************************************/

DVZ_EXPORT void dvz_atlases(DvzContext* ctx, DvzAtlases* atlases);

DVZ_EXPORT void dvz_atlases_destroy(DvzAtlases* atlases);



#endif
