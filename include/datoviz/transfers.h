/*************************************************************************************************/
/*  GPU data transfers interfacing closely with the canvas event loop                            */
/*************************************************************************************************/

#ifndef DVZ_TRANSFERS_HEADER
#define DVZ_TRANSFERS_HEADER

// #include "../include/datoviz/context.h"
#include "../include/datoviz/vklite.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_TRANSFER_DEQ_UL   0
#define DVZ_TRANSFER_DEQ_DL   1
#define DVZ_TRANSFER_DEQ_COPY 2
#define DVZ_TRANSFER_DEQ_EV   3
// Three deq processes: upload/download, copy, event (incl. download_done)
#define DVZ_TRANSFER_PROC_UD  0
#define DVZ_TRANSFER_PROC_CPY 1
#define DVZ_TRANSFER_PROC_EV  2



/*************************************************************************************************/
/*  Transfer enums                                                                               */
/*************************************************************************************************/

// Transfer type.
typedef enum
{
    DVZ_TRANSFER_NONE,

    DVZ_TRANSFER_BUFFER_UPLOAD,
    DVZ_TRANSFER_BUFFER_DOWNLOAD,
    DVZ_TRANSFER_BUFFER_DOWNLOAD_DONE,
    DVZ_TRANSFER_BUFFER_COPY,

    DVZ_TRANSFER_TEXTURE_UPLOAD,
    DVZ_TRANSFER_TEXTURE_DOWNLOAD,
    DVZ_TRANSFER_TEXTURE_DOWNLOAD_DONE,
    DVZ_TRANSFER_TEXTURE_COPY,

    DVZ_TRANSFER_TEXTURE_BUFFER,
    DVZ_TRANSFER_BUFFER_TEXTURE,
} DvzDataTransferType;



/*************************************************************************************************/
/*  Transfer typedefs                                                                            */
/*************************************************************************************************/

typedef struct DvzTransfer DvzTransfer;
typedef struct DvzTransferBuffer DvzTransferBuffer;
typedef struct DvzTransferBufferCopy DvzTransferBufferCopy;
typedef struct DvzTransferTexture DvzTransferTexture;
typedef struct DvzTransferBufferTexture DvzTransferBufferTexture;
typedef struct DvzTransferTextureBuffer DvzTransferTextureBuffer;
typedef struct DvzTransferTextureCopy DvzTransferTextureCopy;
typedef struct DvzTransferDownload DvzTransferDownload;
typedef union DvzTransferUnion DvzTransferUnion;



/*************************************************************************************************/
/*  Transfer structs                                                                             */
/*************************************************************************************************/

struct DvzTransferBuffer
{
    DvzBufferRegions stg, br; // staging (mappable) buffer, and source/destination buffer
    VkDeviceSize stg_offset, br_offset, size;
    void* data;

    // If the data has to be copied to a texture after transfer to the GPU buffer.
    DvzTexture* tex;
    uvec3 tex_offset, shape;
};



struct DvzTransferBufferCopy
{
    DvzBufferRegions src, dst;
    VkDeviceSize src_offset, dst_offset, size;
    void* to_download; // if set, need to enqueue a DL task to this buffer after the copy
};



struct DvzTransferTexture
{
    DvzTexture* tex;
    uvec3 offset, shape;

    DvzBufferRegions stg;
    VkDeviceSize stg_offset;

    VkDeviceSize size;
    void* data;
};



struct DvzTransferTextureCopy
{
    DvzTexture *src, *dst;
    uvec3 src_offset, dst_offset, shape;
    VkDeviceSize size;
    // void* to_download; // if set, need to enqueue a DL task to this buffer after the copy
};



struct DvzTransferBufferTexture
{
    DvzTexture* tex;
    uvec3 tex_offset, shape;
    DvzBufferRegions br;
    VkDeviceSize buf_offset;
    VkDeviceSize size;
    // void* to_download; // if set, need to enqueue a DL task to this buffer after the copy
};



struct DvzTransferTextureBuffer
{
    DvzTexture* tex;
    uvec3 tex_offset, shape;
    DvzBufferRegions br;
    VkDeviceSize buf_offset;
    VkDeviceSize size;
    void* to_download; // if set, need to enqueue a DL task to this buffer after the copy
};



struct DvzTransferDownload
{
    VkDeviceSize size;
    void* data;
};



union DvzTransferUnion
{
    DvzTransferBuffer buf;
    DvzTransferTexture tex;
    DvzTransferBufferCopy buf_copy;
    DvzTransferTextureCopy tex_copy;
    DvzTransferTextureBuffer tex_buf;
    DvzTransferBufferTexture buf_tex;
    DvzTransferDownload download;
};



struct DvzTransfer
{
    DvzDataTransferType type;
    DvzTransferUnion u;
};



/*************************************************************************************************/
/*  Old transfers                                                                                */
/*************************************************************************************************/

/**
 * Upload data to 1 or N buffer regions on the GPU.
 *
 * @param canvas the canvas
 * @param br the buffer regions to update
 * @param offset the offset within the buffer regions, in bytes
 * @param size the size of the data to upload, in bytes
 * @param data pointer to the data to upload to the GPU
 */
DVZ_EXPORT void dvz_upload_buffer(
    DvzContext* ctx, DvzBufferRegions br, VkDeviceSize offset, VkDeviceSize size, void* data);

/**
 * Download data from a buffer region to the CPU.
 *
 * @param canvas the canvas
 * @param br the buffer regions to update
 * @param offset the offset within the buffer regions, in bytes
 * @param size the size of the data to upload, in bytes
 * @param[out] data pointer to a buffer already allocated to contain `size` bytes
 */
DVZ_EXPORT void dvz_download_buffer(
    DvzContext* ctx, DvzBufferRegions br, VkDeviceSize offset, VkDeviceSize size, void* data);

/**
 * Copy data between two GPU buffer regions.
 *
 * This function does not involve GPU-CPU data transfers.
 *
 * @param canvas the canvas
 * @param src the buffer region to copy from
 * @param src_offset the offset within the source buffer region
 * @param dst the buffer region to copy to
 * @param dst_offset the offset within the target buffer region
 * @param size the size of the data to copy
 */
DVZ_EXPORT void dvz_copy_buffer(
    DvzContext* ctx, DvzBufferRegions src, VkDeviceSize src_offset, //
    DvzBufferRegions dst, VkDeviceSize dst_offset, VkDeviceSize size);

/**
 * Upload data to a texture.
 *
 * @param canvas the canvas
 * @param texture the texture to update
 * @param offset the offset within the texture
 * @param shape the shape of the region to update within the texture
 * @param size the size of the uploaded data, in bytes
 * @param data pointer to the data to upload to the GPU
 */
DVZ_EXPORT void dvz_upload_texture(
    DvzContext* ctx, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data);

/**
 * Download data from a texture.
 *
 * @param canvas the canvas
 * @param texture the texture to download from
 * @param offset the offset within the texture
 * @param shape the shape of the region to update within the texture
 * @param size the size of the downloaded data, in bytes
 * @param[out] data pointer to the buffer that will hold the downloaded data
 */
DVZ_EXPORT void dvz_download_texture(
    DvzContext* ctx, DvzTexture* tex, uvec3 offset, uvec3 shape, VkDeviceSize size, void* data);

/**
 * Copy part of a texture to another.
 *
 * This function does not involve GPU-CPU data transfers.
 *
 * @param canvas the canvas
 * @param src the source texture
 * @param src_offset the offset within the source texture
 * @param dst the target texture
 * @param dst_offset the offset within the target texture
 * @param shape the shape of the part of the texture to copy
 * @param size the corresponding size of that part, in bytes
 */
DVZ_EXPORT void dvz_copy_texture(
    DvzContext* ctx, DvzTexture* src, uvec3 src_offset, DvzTexture* dst, uvec3 dst_offset,
    uvec3 shape, VkDeviceSize size);

/**
 * Process the pending transfers.
 *
 * When the event loop is running, all transfers are enqueued in a queue rather than executed
 * directly. The reason is that proper synchronization is required in order to avoid modifying
 GPU
 * objects while they are being used for rendering. The transfer processing function is called
 at a
 * deterministic time within the main event loop.
 *
 * @param canvas the canvas
 * @param br the buffer regions to update
 * @param offset the offset within the buffer regions, in bytes
 * @param size the size of the data to upload, in bytes
 * @param data pointer to the data to upload to the GPU
 */
DVZ_EXPORT void dvz_process_transfers(DvzContext* context);



#endif
