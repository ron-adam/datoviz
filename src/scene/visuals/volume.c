/*
 * Copyright (c) 2021 Cyrille Rossant and contributors. All rights reserved.
 * Licensed under the MIT license. See LICENSE file in the project root for details.
 * SPDX-License-Identifier: MIT
 */


/*  Volume */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Includes                                                                                     */
/*************************************************************************************************/

#include "scene/visuals/volume.h"
#include "../src/resources_utils.h"
#include "datoviz.h"
#include "datoviz_protocol.h"
#include "datoviz_types.h"
#include "fileio.h"
#include "scene/graphics.h"
#include "scene/viewset.h"
#include "scene/visual.h"



/*************************************************************************************************/
/*  Internal functions                                                                           */
/*************************************************************************************************/

static void _visual_callback(
    DvzVisual* visual, DvzId canvas, //
    uint32_t first, uint32_t count,  //
    uint32_t first_instance, uint32_t instance_count)
{
    ANN(visual);
    dvz_visual_instance(visual, canvas, 36 * first, 0, 36 * count, first_instance, instance_count);
}



/*************************************************************************************************/
/*  Functions                                                                                    */
/*************************************************************************************************/

DvzVisual* dvz_volume(DvzBatch* batch, int flags)
{
    ANN(batch);

    DvzVisual* visual = dvz_visual(batch, DVZ_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, flags);
    ANN(visual);

    // Visual shaders.
    dvz_visual_shader(visual, "graphics_volume");

    // Disable depth test.
    dvz_visual_depth(visual, DVZ_DEPTH_TEST_DISABLE);

    // Volume specialization constants.
    volume_specialization(visual);

    // Vertex attributes.
    dvz_visual_attr(visual, 0, FIELD(DvzVolumeVertex, pos), DVZ_FORMAT_R32G32B32_SFLOAT, 0);

    // Vertex stride.
    dvz_visual_stride(visual, 0, sizeof(DvzVolumeVertex));

    // Slots.
    dvz_visual_slot(visual, 0, DVZ_SLOT_DAT);
    dvz_visual_slot(visual, 1, DVZ_SLOT_DAT);
    dvz_visual_slot(visual, 2, DVZ_SLOT_DAT);
    dvz_visual_slot(visual, 3, DVZ_SLOT_TEX);

    // Params.
    DvzParams* params = dvz_visual_params(visual, 2, sizeof(DvzVolumeParams));

    dvz_params_attr(params, 0, FIELD(DvzVolumeParams, box_size));
    dvz_params_attr(params, 1, FIELD(DvzVolumeParams, uvw0));
    dvz_params_attr(params, 2, FIELD(DvzVolumeParams, uvw1));
    dvz_params_attr(params, 3, FIELD(DvzVolumeParams, transfer));

    dvz_visual_param(visual, 2, 0, (vec4){1, 1, 1, 0}); // box_size
    dvz_visual_param(visual, 2, 1, (vec4){0, 0, 0, 0}); // uvw0
    dvz_visual_param(visual, 2, 2, (vec4){1, 1, 1, 0}); // uvw1
    dvz_visual_param(visual, 2, 3, (vec4){1, 0, 0, 0}); // transfer

    // Visual draw callback.
    dvz_visual_callback(visual, _visual_callback);

    return visual;
}



void dvz_volume_alloc(DvzVisual* visual, uint32_t item_count)
{
    ANN(visual);
    log_debug("allocating the volume visual");

    DvzBatch* batch = visual->batch;
    ANN(batch);

    const uint32_t K = 36;

    // Allocate the visual.
    dvz_visual_alloc(visual, item_count, K * item_count, 0);

    // TODO
    float u = .5;
    float x0 = -u, x1 = +u, y0 = -u, y1 = +u, z0 = -u, z1 = +u;

    // Vertex positions.
    vec3* pos = (vec3*)calloc(K * item_count, sizeof(vec3));
    for (uint32_t i = 0; i < item_count; i++)
    {
        memcpy(
            &pos[K * i],
            (vec3[]){
                {x0, y0, z1}, // front
                {x1, y0, z1}, //
                {x1, y1, z1}, //
                {x1, y1, z1}, //
                {x0, y1, z1}, //
                {x0, y0, z1}, //
                              //
                {x1, y0, z1}, // right
                {x1, y0, z0}, //
                {x1, y1, z0}, //
                {x1, y1, z0}, //
                {x1, y1, z1}, //
                {x1, y0, z1}, //
                              //
                {x0, y1, z0}, // back
                {x1, y1, z0}, //
                {x1, y0, z0}, //
                {x1, y0, z0}, //
                {x0, y0, z0}, //
                {x0, y1, z0}, //
                              //
                {x0, y0, z0}, // left
                {x0, y0, z1}, //
                {x0, y1, z1}, //
                {x0, y1, z1}, //
                {x0, y1, z0}, //
                {x0, y0, z0}, //
                              //
                {x0, y0, z0}, // bottom
                {x1, y0, z0}, //
                {x1, y0, z1}, //
                {x1, y0, z1}, //
                {x0, y0, z1}, //
                {x0, y0, z0}, //
                              //
                {x0, y1, z1}, // top
                {x1, y1, z1}, //
                {x1, y1, z0}, //
                {x1, y1, z0}, //
                {x0, y1, z0}, //
                {x0, y1, z1}, //
            },
            K * sizeof(vec3));
    }
    dvz_visual_data(visual, 0, 0, item_count * K, pos);
    FREE(pos);
}



void dvz_volume_texture(
    DvzVisual* visual, DvzId tex, DvzFilter filter, DvzSamplerAddressMode address_mode)
{
    ANN(visual);

    DvzBatch* batch = visual->batch;
    ANN(batch);

    DvzId sampler = dvz_create_sampler(batch, filter, address_mode).id;

    // Bind the texture to the visual.
    dvz_visual_tex(visual, 3, tex, sampler, DVZ_ZERO_OFFSET);
}



void dvz_volume_size(DvzVisual* visual, float w, float h, float d)
{
    ANN(visual);
    dvz_visual_param(visual, 2, 0, (vec4){w, h, d, 0});
}



void dvz_volume_texcoords(DvzVisual* visual, vec3 uvw0, vec3 uvw1)
{
    ANN(visual);
    dvz_visual_param(visual, 2, 1, (vec4){uvw0[0], uvw0[1], uvw0[2], 0});
    dvz_visual_param(visual, 2, 2, (vec4){uvw1[0], uvw1[1], uvw1[2], 0});
}



void dvz_volume_transfer(DvzVisual* visual, vec4 transfer)
{
    ANN(visual);
    dvz_visual_param(visual, 2, 3, transfer);
}



DvzId dvz_tex_volume(
    DvzBatch* batch, DvzFormat format, uint32_t width, uint32_t height, uint32_t depth, void* data)
{
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT(depth > 0);
    ANN(data);

    uvec3 shape = {width, height, depth};
    DvzSize size = width * height * depth * _format_size(format);
    DvzId tex = dvz_create_tex(batch, DVZ_TEX_3D, format, shape, 0).id;
    dvz_upload_tex(batch, tex, DVZ_ZERO_OFFSET, shape, size, data, 0);

    return tex;
}
