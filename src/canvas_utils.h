#ifndef DVZ_CANVAS_UTILS_HEADER
#define DVZ_CANVAS_UTILS_HEADER

#include "../include/datoviz/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

static DvzRenderpass default_renderpass(
    DvzGpu* gpu, VkClearColorValue clear_color_value, VkFormat format, //
    bool overlay, bool pick)
{
    DvzRenderpass renderpass = dvz_renderpass(gpu);

    VkClearValue clear_color = {0};
    clear_color.color = clear_color_value;

    VkClearValue clear_depth = {0};
    clear_depth.depthStencil.depth = 1.0f;

    VkClearValue clear_color_pick = {0};

    dvz_renderpass_clear(&renderpass, clear_color);
    dvz_renderpass_clear(&renderpass, clear_depth);
    if (pick)
        dvz_renderpass_clear(&renderpass, clear_color_pick);

    VkImageLayout layout =
        overlay ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Color attachment.
    dvz_renderpass_attachment(
        &renderpass, 0, //
        DVZ_RENDERPASS_ATTACHMENT_COLOR, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    dvz_renderpass_attachment_layout(&renderpass, 0, VK_IMAGE_LAYOUT_UNDEFINED, layout);
    dvz_renderpass_attachment_ops(
        &renderpass, 0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

    // Depth attachment.
    dvz_renderpass_attachment(
        &renderpass, 1, //
        DVZ_RENDERPASS_ATTACHMENT_DEPTH, VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    dvz_renderpass_attachment_layout(
        &renderpass, 1, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    dvz_renderpass_attachment_ops(
        &renderpass, 1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

    // Pick attachment.
    if (pick)
    {
        dvz_renderpass_attachment(
            &renderpass, 2, //
            DVZ_RENDERPASS_ATTACHMENT_PICK, DVZ_PICK_IMAGE_FORMAT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        dvz_renderpass_attachment_layout(
            &renderpass, 2, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        dvz_renderpass_attachment_ops(
            &renderpass, 2, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
    }

    // Subpass.
    dvz_renderpass_subpass_attachment(&renderpass, 0, 0);
    dvz_renderpass_subpass_attachment(&renderpass, 0, 1);
    if (pick)
        dvz_renderpass_subpass_attachment(&renderpass, 0, 2);
    // dvz_renderpass_subpass_dependency(&renderpass, 0, VK_SUBPASS_EXTERNAL, 0);
    // dvz_renderpass_subpass_dependency_stage(
    //     &renderpass, 0, //
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    // dvz_renderpass_subpass_dependency_access(
    //     &renderpass, 0, //
    //     0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    return renderpass;
}



static DvzRenderpass default_renderpass_overlay(DvzGpu* gpu, VkFormat format, VkImageLayout layout)
{
    DvzRenderpass renderpass = dvz_renderpass(gpu);

    // Color attachment.
    dvz_renderpass_attachment(
        &renderpass, 0, //
        DVZ_RENDERPASS_ATTACHMENT_COLOR, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    dvz_renderpass_attachment_layout(
        &renderpass, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, layout);
    dvz_renderpass_attachment_ops(
        &renderpass, 0, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);

    // Subpass.
    dvz_renderpass_subpass_attachment(&renderpass, 0, 0);
    // dvz_renderpass_subpass_dependency(&renderpass, 0, VK_SUBPASS_EXTERNAL, 0);
    // dvz_renderpass_subpass_dependency_stage(
    //     &renderpass, 0, //
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    // dvz_renderpass_subpass_dependency_access(
    //     &renderpass, 0, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    return renderpass;
}



static void
depth_image(DvzImages* depth_images, DvzRenderpass* renderpass, uint32_t width, uint32_t height)
{
    // Depth attachment
    dvz_images_format(depth_images, renderpass->attachments[1].format);
    dvz_images_size(depth_images, width, height, 1);
    dvz_images_tiling(depth_images, VK_IMAGE_TILING_OPTIMAL);
    dvz_images_usage(depth_images, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    dvz_images_memory(depth_images, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_images_layout(depth_images, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    dvz_images_aspect(depth_images, VK_IMAGE_ASPECT_DEPTH_BIT);
    dvz_images_queue_access(depth_images, 0);
    dvz_images_create(depth_images);
}



static void pick_image(DvzImages* pick, DvzRenderpass* renderpass, uint32_t width, uint32_t height)
{
    dvz_images_format(pick, renderpass->attachments[2].format);
    dvz_images_size(pick, width, height, 1);
    dvz_images_tiling(pick, VK_IMAGE_TILING_OPTIMAL);
    dvz_images_usage(pick, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    dvz_images_memory(pick, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dvz_images_layout(pick, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    dvz_images_aspect(pick, VK_IMAGE_ASPECT_COLOR_BIT);
    dvz_images_queue_access(pick, 0);
    dvz_images_create(pick);
}



#ifdef __cplusplus
}
#endif

#endif
