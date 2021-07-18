/*************************************************************************************************/
/*  Singleton application, managing all GPU objects and windows                                  */
/*  Depends on vulkan/vulkan.h because it stores the Vulkan instance.                            */
/*************************************************************************************************/

#ifndef DVZ_APP_HEADER
#define DVZ_APP_HEADER

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <vulkan/vulkan.h>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_PATH_MAX_LEN 1024



/*************************************************************************************************/
/*  Type definitions */
/*************************************************************************************************/

typedef struct DvzApp DvzApp;



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// Backend.
typedef enum
{
    DVZ_BACKEND_NONE,
    DVZ_BACKEND_GLFW,
    DVZ_BACKEND_OFFSCREEN,
} DvzBackend;



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct DvzApp
{
    DvzObject obj;
    uint32_t n_errors;

    // Backend
    DvzBackend backend;

    // Global clock
    DvzClock clock;
    bool is_running;

    // Vulkan objects.
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    // Containers.
    DvzContainer gpus;
    DvzContainer windows;
    DvzContainer canvases;

    // Threads.
    DvzThread timer_thread;
};



/*************************************************************************************************/
/*  App                                                                                          */
/*************************************************************************************************/

/**
 * Create an application instance.
 *
 * There is typically only one App object in a given application. This object holds a pointer to
 * the Vulkan instance and is responsible for discovering the available GPUs.
 *
 * @param backend the backend
 * @returns a pointer to the created app
 */
DVZ_EXPORT DvzApp* dvz_app(DvzBackend backend);

/**
 * Destroy the application.
 *
 * This function automatically destroys all objects created within the application.
 *
 * @param app the application to destroy
 */
DVZ_EXPORT int dvz_app_destroy(DvzApp* app);

/**
 * Destroy the Dear ImGui global context if it was ever initialized.
 */
// DVZ_EXPORT void dvz_imgui_destroy();



#ifdef __cplusplus
}
#endif

#endif
