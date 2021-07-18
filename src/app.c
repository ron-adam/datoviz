#include "../include/datoviz/vklite.h"
#include "vklite_utils.h"
#include <stdlib.h>



/*************************************************************************************************/
/*  Backend-specific initialization                                                              */
/*************************************************************************************************/

static void _glfw_error(int error_code, const char* description)
{
    log_error("glfw error code #%d: %s", error_code, description);
}



static void backend_init(DvzBackend backend)
{
    switch (backend)
    {
    case DVZ_BACKEND_GLFW:

        log_debug("initialize glfw");
        glfwSetErrorCallback(_glfw_error);
        if (!glfwInit())
        {
            exit(1);
        }

        break;
    default:
        break;
    }
}



static void backend_terminate(DvzBackend backend)
{
    switch (backend)
    {
    case DVZ_BACKEND_GLFW:
        log_debug("terminate glfw");
        glfwTerminate();
        break;
    default:
        break;
    }
}



/*************************************************************************************************/
/*  App                                                                                          */
/*************************************************************************************************/

DvzApp* dvz_app(DvzBackend backend)
{
    log_set_level_env();
    log_debug("create the app with backend %d", backend);

    DvzApp* app = calloc(1, sizeof(DvzApp));
    dvz_obj_init(&app->obj);
    app->obj.type = DVZ_OBJECT_TYPE_APP;

#if SWIFTSHADER
    if (backend != DVZ_BACKEND_OFFSCREEN)
    {
        log_warn("when the library is compiled for switshader, offscreen rendering is mandatory");
        backend = DVZ_BACKEND_OFFSCREEN;
    }
#endif

    // Fill the app.autorun struct with DVZ_RUN_* environment variables.
    // dvz_autorun_env(app);

    // // Take env variable "DVZ_RUN_OFFSCREEN" into account, forcing offscreen backend in this
    // case. if (app->autorun.enable && app->autorun.offscreen)
    // {
    //     log_info("forcing offscreen backend because DVZ_RUN_OFFSCREEN env variable is set");
    //     backend = DVZ_BACKEND_OFFSCREEN;
    // }

    // Backend-specific initialization code.
    app->backend = backend;
    backend_init(backend);

    // Initialize the global clock.
    _clock_init(&app->clock);

    app->gpus = dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzGpu), DVZ_OBJECT_TYPE_GPU);
    app->windows =
        dvz_container(DVZ_CONTAINER_DEFAULT_COUNT, sizeof(DvzWindow), DVZ_OBJECT_TYPE_WINDOW);

    // Which extensions are required? Depends on the backend.
    uint32_t required_extension_count = 0;
    const char** required_extensions = backend_extensions(backend, &required_extension_count);

    // Create the instance.
    create_instance(
        required_extension_count, required_extensions, &app->instance, &app->debug_messenger,
        &app->n_errors);
    // debug_messenger != VK_NULL_HANDLE means validation enabled
    dvz_obj_created(&app->obj);

    // Count the number of devices.
    uint32_t gpu_count = 0;
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(app->instance, &gpu_count, NULL));
    log_trace("found %d GPU(s)", gpu_count);
    if (gpu_count == 0)
    {
        log_error("no compatible device found! aborting");
        exit(1);
    }

    // Discover the available GPUs.
    // ----------------------------
    {
        // Initialize the GPU(s).
        VkPhysicalDevice* physical_devices = calloc(gpu_count, sizeof(VkPhysicalDevice));
        VK_CHECK_RESULT(vkEnumeratePhysicalDevices(app->instance, &gpu_count, physical_devices));
        ASSERT(gpu_count <= DVZ_CONTAINER_DEFAULT_COUNT);
        DvzGpu* gpu = NULL;
        for (uint32_t i = 0; i < gpu_count; i++)
        {
            gpu = dvz_container_alloc(&app->gpus);
            dvz_obj_init(&gpu->obj);
            gpu->app = app;
            gpu->idx = i;
            discover_gpu(physical_devices[i], gpu);
            log_debug("found device #%d: %s", gpu->idx, gpu->name);
        }

        FREE(physical_devices);
    }

    return app;
}



int dvz_app_destroy(DvzApp* app)
{
    ASSERT(app != NULL);

    log_debug("destroy the app with backend %d", app->backend);
    dvz_app_wait(app);

    // Destroy the canvases.
    dvz_canvases_destroy(&app->canvases);

    // Destroy the GPUs.
    CONTAINER_DESTROY_ITEMS(DvzGpu, app->gpus, dvz_gpu_destroy)
    dvz_container_destroy(&app->gpus);

    // Destroy the windows.
    CONTAINER_DESTROY_ITEMS(DvzWindow, app->windows, dvz_window_destroy)
    dvz_container_destroy(&app->windows);

    // Destroy the debug messenger.
    if (app->debug_messenger)
    {
        destroy_debug_utils_messenger_EXT(app->instance, app->debug_messenger, NULL);
        app->debug_messenger = NULL;
    }

    // Destroy the instance.
    log_trace("destroy Vulkan instance");
    if (app->instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(app->instance, NULL);
        app->instance = 0;
    }

    // Backend-specific termination code.
    backend_terminate(app->backend);

    // Free the App memory.
    int res = (int)app->n_errors;
    FREE(app);
    log_trace("app destroyed");

    return res;
}
