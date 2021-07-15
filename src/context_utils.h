#ifndef DVZ_CONTEXT_UTILS_HEADER
#define DVZ_CONTEXT_UTILS_HEADER

#include "../include/datoviz/context.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Staging buffer                                                                               */
/*************************************************************************************************/

// Get the staging buffer, and make sure it can contain `size` bytes.
static DvzBuffer* staging_buffer(DvzContext* context, VkDeviceSize size)
{
    log_trace("requesting staging buffer of size %s", pretty_size(size));
    DvzBuffer* staging = (DvzBuffer*)dvz_container_get(&context->buffers, DVZ_BUFFER_TYPE_STAGING);
    ASSERT(staging != NULL);
    ASSERT(staging->buffer != VK_NULL_HANDLE);

    // Make sure the staging buffer is idle before using it.
    // TODO: optimize this and avoid hard synchronization here before copying data into
    // the staging buffer.
    // dvz_queue_wait(context->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // Resize the staging buffer is needed.
    // TODO: keep staging buffer fixed and copy parts of the data to staging buffer in several
    // steps?
    if (staging->size < size)
    {
        VkDeviceSize new_size = dvz_next_pow2(size);
        log_debug("reallocating staging buffer to %s", pretty_size(new_size));
        dvz_buffer_resize(staging, new_size);
    }
    ASSERT(staging->size >= size);
    return staging;
}



static void _copy_buffer_from_staging(
    DvzContext* context, DvzBufferRegions br, VkDeviceSize offset, VkDeviceSize size)
{
    ASSERT(context != NULL);

    DvzGpu* gpu = context->gpu;
    ASSERT(gpu != NULL);

    DvzBuffer* staging = staging_buffer(context, size);
    ASSERT(staging != NULL);

    // Take transfer cmd buf.
    DvzCommands cmds_ = dvz_commands(gpu, 0, 1);
    DvzCommands* cmds = &cmds_;
    dvz_cmd_reset(cmds, 0);
    dvz_cmd_begin(cmds, 0);

    VkBufferCopy region = {0};
    region.size = size;
    region.srcOffset = 0;
    region.dstOffset = br.offsets[0] + offset;
    vkCmdCopyBuffer(cmds->cmds[0], staging->buffer, br.buffer->buffer, br.count, &region);
    dvz_cmd_end(cmds, 0);

    // Wait for the render queue to be idle.
    // TODO: less brutal synchronization with semaphores. Here we stop all
    // rendering so that we're sure that the buffer we're going to write to is not
    // being used by the GPU.
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_RENDER);

    // Submit the commands to the transfer queue.
    DvzSubmit submit = dvz_submit(gpu);
    dvz_submit_commands(&submit, cmds);
    log_debug("copy %s from staging buffer", pretty_size(size));
    dvz_submit_send(&submit, 0, NULL, 0);

    // Wait for the transfer queue to be idle.
    // TODO: less brutal synchronization with semaphores. Here we wait for the
    // transfer to be complete before we send new rendering commands.
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
}



static void _copy_buffer_to_staging(
    DvzContext* context, DvzBufferRegions br, VkDeviceSize offset, VkDeviceSize size)
{
    ASSERT(context != NULL);

    DvzGpu* gpu = context->gpu;
    ASSERT(gpu != NULL);

    DvzBuffer* staging = staging_buffer(context, size);
    ASSERT(staging != NULL);

    // Take transfer cmd buf.
    DvzCommands cmds_ = dvz_commands(gpu, 0, 1);
    DvzCommands* cmds = &cmds_;
    dvz_cmd_reset(cmds, 0);
    dvz_cmd_begin(cmds, 0);

    // Determine the offset in the source buffer.
    // Should be consecutive offsets.
    VkDeviceSize vk_offset = br.offsets[0];
    uint32_t n_regions = br.count;
    for (uint32_t i = 1; i < n_regions; i++)
    {
        ASSERT(br.offsets[i] == vk_offset + i * size);
    }
    // Take into account the transfer offset.
    vk_offset += offset;

    // Copy to staging buffer
    ASSERT(br.buffer != 0);
    dvz_cmd_copy_buffer(cmds, 0, br.buffer, vk_offset, staging, 0, size * n_regions);
    dvz_cmd_end(cmds, 0);

    // Wait for the compute queue to be idle, as we assume the buffer to be copied from may
    // be modified by compute shaders.
    // TODO: more efficient synchronization (semaphores?)
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_COMPUTE);

    // Submit the commands to the transfer queue.
    DvzSubmit submit = dvz_submit(gpu);
    dvz_submit_commands(&submit, cmds);
    log_debug("copy %s to staging buffer", pretty_size(size));
    dvz_submit_send(&submit, 0, NULL, 0);

    // Wait for the transfer queue to be idle.
    // TODO: less brutal synchronization with semaphores. Here we wait for the
    // transfer to be complete before we send new rendering commands.
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
}



static void _copy_texture_from_staging(
    DvzContext* context, DvzTexture* texture, uvec3 offset, uvec3 shape, VkDeviceSize size)
{
    ASSERT(context != NULL);

    DvzGpu* gpu = context->gpu;
    ASSERT(gpu != NULL);

    DvzBuffer* staging = staging_buffer(context, size);
    ASSERT(staging != NULL);

    // Take transfer cmd buf.
    DvzCommands cmds_ = dvz_commands(gpu, 0, 1);
    DvzCommands* cmds = &cmds_;
    dvz_cmd_reset(cmds, 0);
    dvz_cmd_begin(cmds, 0);

    // Image transition.
    DvzBarrier barrier = dvz_barrier(gpu);
    dvz_barrier_stages(&barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    ASSERT(texture != NULL);
    ASSERT(texture->image != NULL);
    dvz_barrier_images(&barrier, texture->image);
    dvz_barrier_images_layout(
        &barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    dvz_barrier_images_access(&barrier, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    dvz_cmd_barrier(cmds, 0, &barrier);

    // Copy to staging buffer
    dvz_cmd_copy_buffer_to_image(cmds, 0, staging, texture->image);

    // Image transition.
    dvz_barrier_images_layout(
        &barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->image->layout);
    dvz_barrier_images_access(&barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
    dvz_cmd_barrier(cmds, 0, &barrier);

    dvz_cmd_end(cmds, 0);

    // Wait for the render queue to be idle.
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_RENDER);

    // Submit the commands to the transfer queue.
    DvzSubmit submit = dvz_submit(gpu);
    dvz_submit_commands(&submit, cmds);
    dvz_submit_send(&submit, 0, NULL, 0);

    // Wait for the transfer queue to be idle.
    // TODO: less brutal synchronization with semaphores. Here we wait for the
    // transfer to be complete before we send new rendering commands.
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
}



static void _copy_texture_to_staging(
    DvzContext* context, DvzTexture* texture, uvec3 offset, uvec3 shape, VkDeviceSize size)
{
    ASSERT(context != NULL);

    DvzGpu* gpu = context->gpu;
    ASSERT(gpu != NULL);

    DvzBuffer* staging = staging_buffer(context, size);
    ASSERT(staging != NULL);

    // Take transfer cmd buf.
    DvzCommands cmds_ = dvz_commands(gpu, 0, 1);
    DvzCommands* cmds = &cmds_;
    dvz_cmd_reset(cmds, 0);
    dvz_cmd_begin(cmds, 0);

    // Image transition.
    DvzBarrier barrier = dvz_barrier(gpu);
    dvz_barrier_stages(&barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    ASSERT(texture != NULL);
    ASSERT(texture->image != NULL);
    dvz_barrier_images(&barrier, texture->image);
    dvz_barrier_images_layout(
        &barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    dvz_barrier_images_access(&barrier, 0, VK_ACCESS_TRANSFER_READ_BIT);
    dvz_cmd_barrier(cmds, 0, &barrier);

    // Copy to staging buffer
    dvz_cmd_copy_image_to_buffer(cmds, 0, texture->image, staging);

    // Image transition.
    dvz_barrier_images_layout(
        &barrier, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image->layout);
    dvz_barrier_images_access(&barrier, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT);
    dvz_cmd_barrier(cmds, 0, &barrier);

    dvz_cmd_end(cmds, 0);

    // Wait for the render queue to be idle.
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_RENDER);

    // Submit the commands to the transfer queue.
    DvzSubmit submit = dvz_submit(gpu);
    dvz_submit_commands(&submit, cmds);
    dvz_submit_send(&submit, 0, NULL, 0);

    // Wait for the transfer queue to be idle.
    // TODO: less brutal synchronization with semaphores. Here we wait for the
    // transfer to be complete before we send new rendering commands.
    // dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);
}



/*************************************************************************************************/
/*  Default resources                                                                            */
/*************************************************************************************************/

static DvzTexture* _default_transfer_texture(DvzContext* context)
{
    ASSERT(context != NULL);
    DvzGpu* gpu = context->gpu;
    ASSERT(gpu != NULL);

    uvec3 shape = {256, 1, 1};
    DvzTexture* texture = dvz_ctx_texture(context, 1, shape, VK_FORMAT_R32_SFLOAT);
    float* tex_data = (float*)calloc(256, sizeof(float));
    for (uint32_t i = 0; i < 256; i++)
        tex_data[i] = i / 255.0;
    dvz_texture_address_mode(texture, DVZ_TEXTURE_AXIS_U, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    uvec3 offset = {0, 0, 0};

    dvz_texture_upload(texture, offset, offset, 256 * sizeof(float), tex_data);
    dvz_queue_wait(gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    FREE(tex_data);
    return texture;
}



/*************************************************************************************************/
/*  New transfers                                                                                */
/*************************************************************************************************/


static void* _transfer_loop(DvzContext* ctx, uint32_t proc_idx)
{
    ASSERT(ctx != NULL);
    DvzDeqItem item = {0};

    // These are the two queues that should be dequeued in the transfer thread. The two other
    // queues, the copy thread, and the event thread, are to be dequeued by the main thread
    // instead, in the main event loop.
    while (true)
    {
        log_trace("waiting for proc #%d", proc_idx);
        // This call dequeues an item and also calls all registered callbacks if the item is not
        // null.
        item = dvz_deq_dequeue(&ctx->deq, proc_idx, true);
        if (item.item == NULL)
        {
            log_debug("stop the transfer loop for proc #%d", proc_idx);
            break;
        }
        else
        {
            // WARNING: the DvzTransfer pointer MUST be alloc-ed on the heap, because it is always
            // freed here after dequeue and callbacks.
            log_trace("free item");
            FREE(item.item);
        }
        log_trace("got a deq item on proc #%d", proc_idx);
    }
    return NULL;
}

// Process for the deq proc #0, which encompasses the two queues UPLOAD and DOWNLOAD.
static void* _thread_transfers(void* user_data)
{
    DvzContext* ctx = (DvzContext*)user_data;
    return _transfer_loop(ctx, DVZ_CTX_DEQ_PUD);
}



// Get the staging buffer, and make sure it can contain `size` bytes.
static DvzBuffer* _staging_buffer(DvzContext* context, VkDeviceSize size)
{
    log_trace("requesting staging buffer of size %s", pretty_size(size));
    DvzBuffer* staging = (DvzBuffer*)dvz_container_get(&context->buffers, DVZ_BUFFER_TYPE_STAGING);
    ASSERT(staging != NULL);
    ASSERT(staging->buffer != VK_NULL_HANDLE);

    // Resize the staging buffer is needed.
    // TODO: keep staging buffer fixed and copy parts of the data to staging buffer in several
    // steps?
    if (staging->size < size)
    {
        VkDeviceSize new_size = dvz_next_pow2(size);
        log_debug("reallocating staging buffer to %s", pretty_size(new_size));
        dvz_buffer_resize(staging, new_size);
    }
    ASSERT(staging->size >= size);
    return staging;
}



static void _enqueue_buffer_upload(
    DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, DvzBufferRegions stg,
    VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    DvzTransfer* tr = calloc(1, sizeof(DvzTransfer));
    tr->type = DVZ_TRANSFER_BUFFER_UPLOAD;
    tr->u.buf.stg = stg;
    tr->u.buf.stg_offset = stg_offset;
    tr->u.buf.br = br;
    tr->u.buf.br_offset = br_offset;
    tr->u.buf.size = size;
    tr->u.buf.data = data;
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_UL, tr->type, tr);
}

static void _enqueue_buffer_download(
    DvzDeq* deq, DvzBufferRegions br, VkDeviceSize br_offset, DvzBufferRegions stg,
    VkDeviceSize stg_offset, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer download");

    DvzTransfer* trd = calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trd->type = DVZ_TRANSFER_BUFFER_DOWNLOAD;
    trd->u.buf.br = br;
    trd->u.buf.br_offset = br_offset;
    trd->u.buf.stg = stg;
    trd->u.buf.stg_offset = stg_offset;
    trd->u.buf.size = size;
    trd->u.buf.data = data;
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_DL, trd->type, trd);
}

static void _enqueue_buffer_copy(
    DvzDeq* deq, DvzBufferRegions src, VkDeviceSize src_offset, DvzBufferRegions dst,
    VkDeviceSize dst_offset, VkDeviceSize size, void* to_download)
{
    ASSERT(deq != NULL);
    ASSERT(src.buffer != NULL);
    ASSERT(dst.buffer != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer copy");

    DvzTransfer* trc = calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trc->type = DVZ_TRANSFER_BUFFER_COPY;
    trc->u.buf_copy.src = src;
    trc->u.buf_copy.src_offset = src_offset;
    trc->u.buf_copy.dst = dst;
    trc->u.buf_copy.dst_offset = dst_offset;
    trc->u.buf_copy.size = size;
    trc->u.buf_copy.to_download = to_download;
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_COPY, trc->type, trc);
}

static void _enqueue_buffer_download_done(DvzDeq* deq, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue buffer download done");

    DvzTransfer* trd = calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trd->type = DVZ_TRANSFER_BUFFER_DOWNLOAD_DONE;
    trd->u.download.size = size;
    trd->u.download.data = data;
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_EV, trd->type, trd);
}



static void _transfer_buffer_upload(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_BUFFER_UPLOAD);
    log_trace("process buffer upload");

    DvzTransferBuffer* trb = &tr->u.buf;

    // Copy the data to the staging buffer.
    ASSERT(trb->stg.buffer != NULL);
    ASSERT(trb->stg.size > 0);
    ASSERT(trb->size > 0);
    ASSERT(trb->stg_offset + trb->size <= trb->stg.size);

    // Take offset and size into account in the staging buffer.
    // NOTE: this call blocks while the data is being copied from CPU to GPU (mapped memcpy).
    dvz_buffer_regions_upload(&trb->stg, 0, trb->stg_offset, trb->size, trb->data);

    // Once the data has been transferred, enqueue a copy task from the staging buffer to the
    // destination buffer.
    if (trb->br.buffer != NULL)
    {
        _enqueue_buffer_copy(
            deq, trb->stg, trb->stg_offset, trb->br, trb->br_offset, trb->size, NULL);
    }
}

static void _transfer_buffer_download(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_BUFFER_DOWNLOAD);
    log_trace("process buffer download");

    DvzTransferBuffer* trb = &tr->u.buf;

    // Copy the data to the staging buffer.
    ASSERT(trb->stg.buffer != NULL);
    ASSERT(trb->stg.size > 0);
    ASSERT(trb->size > 0);
    ASSERT(trb->stg_offset + trb->size <= trb->stg.size);

    // If the data is to be downloaded from a non-mappable buffer, we need to enqueue a copy and
    // stop here the download callback. We'll specify, in the copy task, that the download must be
    // enqueued afterwards.
    if (trb->br.buffer != NULL)
    {
        _enqueue_buffer_copy(
            deq, trb->br, trb->br_offset, trb->stg, trb->stg_offset, trb->size, trb->data);
    }

    // If the data is to be downloaded from a mappable (staging) buffer, we can download the data
    // directly here (blocking call).
    else
    {
        // Take offset and size into account in the staging buffer.
        // NOTE: this call blocks while the data is being copied from GPU to CPU (mapped memcpy).
        dvz_buffer_regions_download(&trb->stg, 0, trb->stg_offset, trb->size, trb->data);

        // Raise a DOWNLOAD_DONE event when the download has finished.
        _enqueue_buffer_download_done(deq, trb->size, trb->data);
    }
}

static void _transfer_buffer_copy(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(user_data != NULL);
    DvzContext* ctx = (DvzContext*)user_data;
    log_trace("process buffer copy");

    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_BUFFER_COPY);

    DvzTransferBufferCopy* trb = &tr->u.buf_copy;

    // Make the GPU-GPU buffer copy (block the GPU and wait for the copy to finish).
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_buffer_regions_copy(&trb->src, trb->src_offset, &trb->dst, trb->dst_offset, trb->size);
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // If needed, enqueue a download task after the copy. The destination buffer of the copy then
    // needs to be a mappable buffer (typically, a staging buffer).
    if (trb->to_download != NULL)
    {
        _enqueue_buffer_download(
            deq, (DvzBufferRegions){0}, 0, trb->dst, trb->dst_offset, trb->size, trb->to_download);
    }
}



static void _texture_shape(DvzTexture* texture, uvec3 shape)
{
    ASSERT(texture != NULL);
    ASSERT(texture->image != NULL);

    if (shape[0] == 0)
        shape[0] = texture->image->width;
    if (shape[1] == 0)
        shape[1] = texture->image->height;
    if (shape[2] == 0)
        shape[2] = texture->image->depth;
}



static void _enqueue_texture_upload(
    DvzDeq* deq, DvzTexture* tex, uvec3 tex_offset, DvzTexture* stg, uvec3 stg_offset, uvec3 shape,
    VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    DvzTransfer* tr = calloc(1, sizeof(DvzTransfer));
    tr->type = DVZ_TRANSFER_TEXTURE_UPLOAD;
    tr->u.tex.staging = stg;
    // tr->u.tex.stg_offset = stg_offset; // TODO
    tr->u.tex.texture = tex;
    memcpy(tr->u.tex.offset, tex_offset, sizeof(uvec3));
    memcpy(tr->u.tex.shape, shape, sizeof(uvec3));
    tr->u.tex.size = size;
    tr->u.tex.data = data;
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_UL, tr->type, tr);
}

static void _enqueue_texture_download(
    DvzDeq* deq, DvzTexture* tex, uvec3 tex_offset, DvzTexture* stg, uvec3 stg_offset, uvec3 shape,
    VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(stg != NULL);
    log_trace("enqueue texture download");

    DvzTransfer* trd = calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trd->type = DVZ_TRANSFER_TEXTURE_DOWNLOAD;
    trd->u.tex.texture = tex;
    trd->u.tex.staging = stg;
    // trd->u.tex. = stg_offset; // TODO
    memcpy(trd->u.tex.offset, tex_offset, sizeof(uvec3));
    memcpy(trd->u.tex.shape, shape, sizeof(uvec3));
    trd->u.tex.size = size;
    trd->u.tex.data = data;
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_DL, trd->type, trd);
}

static void _enqueue_texture_copy(
    DvzDeq* deq, DvzTexture* src, uvec3 src_offset, DvzTexture* dst, uvec3 dst_offset, uvec3 shape,
    VkDeviceSize size, void* to_download)
{
    ASSERT(deq != NULL);
    ASSERT(src != NULL);
    ASSERT(dst != NULL);
    log_trace("enqueue texture copy");

    DvzTransfer* trc = calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trc->type = DVZ_TRANSFER_TEXTURE_COPY;
    trc->u.tex_copy.src = src;
    trc->u.tex_copy.dst = dst;
    trc->u.tex_copy.size = size;
    trc->u.tex_copy.to_download = to_download;
    memcpy(trc->u.tex_copy.src_offset, src_offset, sizeof(uvec3));
    memcpy(trc->u.tex_copy.dst_offset, dst_offset, sizeof(uvec3));
    memcpy(trc->u.tex_copy.shape, shape, sizeof(uvec3));
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_COPY, trc->type, trc);
}

static void _enqueue_texture_download_done(DvzDeq* deq, VkDeviceSize size, void* data)
{
    ASSERT(deq != NULL);
    ASSERT(size > 0);
    log_trace("enqueue texture download done");

    DvzTransfer* trd = calloc(1, sizeof(DvzTransfer)); // will be free-ed by the callbacks
    trd->type = DVZ_TRANSFER_TEXTURE_DOWNLOAD_DONE;
    trd->u.download.size = size;
    trd->u.download.data = data;
    dvz_deq_enqueue(deq, DVZ_CTX_DEQ_EV, trd->type, trd);
}



static void _transfer_texture_upload(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_TEXTURE_UPLOAD);
    log_trace("process texture upload");

    DvzTransferTexture* trb = &tr->u.tex;

    // Copy the data to the staging buffer.
    ASSERT(trb->staging != NULL);
    ASSERT(trb->size > 0);

    // Take offset and size into account in the staging buffer.
    // NOTE: this call blocks while the data is being copied from CPU to GPU (mapped memcpy).
    // TODO: staging offset
    dvz_texture_upload(trb->staging, (uvec3){0}, trb->shape, trb->size, trb->data);

    // Once the data has been transferred, enqueue a copy task from the staging buffer to the
    // destination buffer.
    if (trb->texture != NULL)
    {
        _enqueue_texture_copy(
            deq, trb->staging, (uvec3){0}, trb->texture, trb->offset, trb->shape, trb->size, NULL);
    }
}

static void _transfer_texture_download(DvzDeq* deq, void* item, void* user_data)
{
    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_TEXTURE_DOWNLOAD);
    log_trace("process texture download");

    DvzTransferTexture* trb = &tr->u.tex;

    // Copy the data to the staging buffer.
    ASSERT(trb->staging != NULL);
    ASSERT(trb->size > 0);

    // If the data is to be downloaded from a non-mappable buffer, we need to enqueue a copy and
    // stop here the download callback. We'll specify, in the copy task, that the download must be
    // enqueued afterwards.
    if (trb->texture != NULL)
    {
        _enqueue_texture_copy(
            deq, trb->texture, trb->offset, trb->staging, (uvec3){0}, trb->shape, trb->size,
            trb->data);
    }

    // If the data is to be downloaded from a mappable (staging) buffer, we can download the data
    // directly here (blocking call).
    else
    {
        // Take offset and size into account in the staging buffer.
        // NOTE: this call blocks while the data is being copied from GPU to CPU (mapped memcpy).
        dvz_texture_download(trb->staging, (uvec3){0}, trb->shape, trb->size, trb->data);

        // Raise a DOWNLOAD_DONE event when the download has finished.
        _enqueue_texture_download_done(deq, trb->size, trb->data);
    }
}

static void _transfer_texture_copy(DvzDeq* deq, void* item, void* user_data)
{
    ASSERT(user_data != NULL);
    DvzContext* ctx = (DvzContext*)user_data;
    log_trace("process texture copy");

    DvzTransfer* tr = (DvzTransfer*)item;
    ASSERT(tr != NULL);
    ASSERT(tr->type == DVZ_TRANSFER_TEXTURE_COPY);

    DvzTransferTextureCopy* trb = &tr->u.tex_copy;

    // Make the GPU-GPU buffer copy (block the GPU and wait for the copy to finish).
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_RENDER);
    dvz_texture_copy(trb->src, trb->src_offset, trb->dst, trb->dst_offset, trb->shape);
    dvz_queue_wait(ctx->gpu, DVZ_DEFAULT_QUEUE_TRANSFER);

    // If needed, enqueue a download task after the copy. The destination buffer of the copy then
    // needs to be a mappable buffer (typically, a staging buffer).
    if (trb->to_download != NULL)
    {
        _enqueue_texture_download(
            deq, NULL, trb->src_offset, trb->dst, trb->dst_offset, trb->shape, trb->size,
            trb->to_download);
    }
}



#ifdef __cplusplus
}
#endif

#endif
