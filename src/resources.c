#include "../include/datoviz/resources.h"
#include "resources_utils.h"
#include "vklite_utils.h"
#include <stdlib.h>



/*************************************************************************************************/
/*  Resource utils                                                                               */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Resources                                                                                    */
/*************************************************************************************************/

void dvz_resources(DvzGpu* gpu, DvzResources* res)
{
    ASSERT(gpu != NULL);
    ASSERT(dvz_obj_is_created(&gpu->obj));
    ASSERT(res != NULL);
    ASSERT(!dvz_obj_is_created(&res->obj));
    // NOTE: this function should only be called once, at context creation.

    log_trace("creating resources");

    // Create the resources.
    res->gpu = gpu;

    // Allocate memory for buffers, textures, and computes.
    _create_resources(res);

    // Create the default resources.
    _default_resources(res);

    dvz_obj_created(&res->obj);
}



void dvz_resources_destroy(DvzResources* res)
{
    if (res == NULL)
    {
        log_error("skip destruction of null resources");
        return;
    }
    log_trace("destroying resources");
    ASSERT(res != NULL);
    ASSERT(res->gpu != NULL);

    // Destroy the font atlas.
    // TODO
    // dvz_font_atlas_destroy(&resources->font_atlas);

    // Destroy the resources.
    _destroy_resources(res);

    // Free the allocated memory.
    dvz_container_destroy(&res->buffers);
    dvz_container_destroy(&res->images);
    dvz_container_destroy(&res->dats);
    dvz_container_destroy(&res->texs);
    dvz_container_destroy(&res->samplers);
    dvz_container_destroy(&res->computes);

    dvz_obj_destroyed(&res->obj);
}
