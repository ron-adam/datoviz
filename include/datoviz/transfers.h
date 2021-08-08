/*************************************************************************************************/
/*  GPU data transfers interfacing closely with the canvas event loop                            */
/*************************************************************************************************/

#ifndef DVZ_TRANSFERS_HEADER
#define DVZ_TRANSFERS_HEADER

#include "../include/datoviz/fifo.h"
#include "../include/datoviz/vklite.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define DVZ_TRANSFER_DEQ_UL   0
#define DVZ_TRANSFER_DEQ_DL   1
#define DVZ_TRANSFER_DEQ_COPY 2
#define DVZ_TRANSFER_DEQ_EV   3
#define DVZ_TRANSFER_DEQ_DUP  4

// Three deq processes: upload/download, copy, event (incl. download_done)
#define DVZ_TRANSFER_PROC_UD  0
#define DVZ_TRANSFER_PROC_CPY 1
#define DVZ_TRANSFER_PROC_EV  2
#define DVZ_TRANSFER_PROC_DUP 3

// Maximum number of pending dup transfers.
#define DVZ_DUPS_MAX 16



/*************************************************************************************************/
/*  Transfer enums                                                                               */
/*************************************************************************************************/

// Transfer type.
typedef enum
{
    DVZ_TRANSFER_NONE,

    DVZ_TRANSFER_BUFFER_UPLOAD,
    DVZ_TRANSFER_BUFFER_DOWNLOAD,
    DVZ_TRANSFER_BUFFER_COPY,

    DVZ_TRANSFER_IMAGE_COPY,
    DVZ_TRANSFER_IMAGE_BUFFER,
    DVZ_TRANSFER_BUFFER_IMAGE,

    DVZ_TRANSFER_DOWNLOAD_DONE, // download is only possible from a buffer

    DVZ_TRANSFER_DUP_UPLOAD,
} DvzDataTransferType;



/*************************************************************************************************/
/*  Transfer typedefs                                                                            */
/*************************************************************************************************/

typedef struct DvzTransfer DvzTransfer;
typedef struct DvzTransferBuffer DvzTransferBuffer;
typedef struct DvzTransferBufferCopy DvzTransferBufferCopy;
typedef struct DvzTransferBufferImage DvzTransferBufferImage;
typedef struct DvzTransferImageCopy DvzTransferImageCopy;
typedef struct DvzTransferDownload DvzTransferDownload;
typedef struct DvzTransferDup DvzTransferDup;
typedef union DvzTransferUnion DvzTransferUnion;
typedef struct DvzTransfers DvzTransfers;
typedef struct DvzTransferDupItem DvzTransferDupItem;
typedef struct DvzTransferDups DvzTransferDups;



/*************************************************************************************************/
/*  Transfer structs                                                                             */
/*************************************************************************************************/

struct DvzTransferBuffer
{
    DvzBufferRegions br;
    VkDeviceSize offset;
    VkDeviceSize size;
    void* data;
};



struct DvzTransferBufferCopy
{
    DvzBufferRegions src, dst;
    VkDeviceSize src_offset, dst_offset, size;
};



struct DvzTransferImageCopy
{
    DvzImages *src, *dst;
    uvec3 src_offset, dst_offset, shape;
    VkDeviceSize size;
};



struct DvzTransferBufferImage
{
    DvzImages* img;
    uvec3 img_offset, shape;
    DvzBufferRegions br;
    VkDeviceSize buf_offset;
    VkDeviceSize size;
};



struct DvzTransferDownload
{
    VkDeviceSize size;
    void* data;
};



struct DvzTransferDup
{
    DvzBufferRegions br, stg;
    VkDeviceSize offset;
    VkDeviceSize size;
    void* data;
    bool recurrent;
};



union DvzTransferUnion
{
    DvzTransferBuffer buf;
    DvzTransferBufferCopy buf_copy;
    DvzTransferImageCopy img_copy;
    DvzTransferBufferImage buf_img;
    DvzTransferDownload download;
    DvzTransferDup dup;
};



struct DvzTransfer
{
    DvzDataTransferType type;
    DvzTransferUnion u;
};



/*************************************************************************************************/
/*  Transfer dups                                                                                */
/*************************************************************************************************/

struct DvzTransferDupItem
{
    bool is_set;
    DvzTransferDup tr;
    bool done[DVZ_MAX_BUFFER_REGIONS_PER_SET];
    // bool mappable;
    // DvzBufferRegions br;
    // VkDeviceSize offset, size;
    // bool recurrent;
};



struct DvzTransferDups
{
    uint32_t count;
    DvzTransferDupItem dups[DVZ_DUPS_MAX];
};



/*************************************************************************************************/
/*  Transfers struct                                                                             */
/*************************************************************************************************/

struct DvzTransfers
{
    DvzObject obj;
    DvzGpu* gpu;

    DvzDeq deq;       // transfer dequeues
    DvzThread thread; // transfer thread

    DvzTransferDups dups;
};



/*************************************************************************************************/
/*  Transfers                                                                                    */
/*************************************************************************************************/

// TODO: docstrings

DVZ_EXPORT void dvz_transfers(DvzGpu* gpu, DvzTransfers* transfers);

DVZ_EXPORT void dvz_transfers_frame(DvzTransfers* transfers, uint32_t img_idx);

/**
 * Destroy a transfers object.
 *
 * @param transfers the DvzTransfers pointer
 */
DVZ_EXPORT void dvz_transfers_destroy(DvzTransfers* transfers);



/*************************************************************************************************/
/*  Convenient but slow transfer functions, essentially used in testing or offscreen settings    */
/*************************************************************************************************/

// WARNING: do not use except for offscreen/testing.

/**
 * Upload data to 1 or N buffer regions on the GPU.
 *
 * @param transfers the DvzTransfers pointer
 * @param br the buffer regions to update
 * @param offset the offset within the buffer regions, in bytes
 * @param size the size of the data to upload, in bytes
 * @param data pointer to the data to upload to the GPU
 */
DVZ_EXPORT void dvz_upload_buffer(
    DvzTransfers* transfers, DvzBufferRegions br, //
    VkDeviceSize offset, VkDeviceSize size, void* data);

/**
 * Download data from a buffer region to the CPU.
 *
 * @param transfers the DvzTransfers pointer
 * @param br the buffer regions to update
 * @param offset the offset within the buffer regions, in bytes
 * @param size the size of the data to upload, in bytes
 * @param[out] data pointer to a buffer already allocated to contain `size` bytes
 */
DVZ_EXPORT void dvz_download_buffer(
    DvzTransfers* transfers, DvzBufferRegions br, //
    VkDeviceSize offset, VkDeviceSize size, void* data);

/**
 * Copy data between two GPU buffer regions.
 *
 * This function does not involve GPU-CPU data transfers.
 *
 * @param transfers the DvzTransfers pointer
 * @param src the buffer region to copy from
 * @param src_offset the offset within the source buffer region
 * @param dst the buffer region to copy to
 * @param dst_offset the offset within the target buffer region
 * @param size the size of the data to copy
 */
DVZ_EXPORT void dvz_copy_buffer(
    DvzTransfers* transfers, DvzBufferRegions src, VkDeviceSize src_offset, //
    DvzBufferRegions dst, VkDeviceSize dst_offset, VkDeviceSize size);



/**
 * Upload data to a image.
 *
 * @param transfers the DvzTransfers pointer
 * @param img the image to update
 * @param offset the offset within the image
 * @param shape the shape of the region to update within the image
 * @param size the size of the uploaded data, in bytes
 * @param data pointer to the data to upload to the GPU
 */
DVZ_EXPORT void dvz_upload_image(
    DvzTransfers* transfers, DvzImages* img, //
    uvec3 offset, uvec3 shape, VkDeviceSize size, void* data);

/**
 * Download data from a image.
 *
 * @param transfers the DvzTransfers pointer
 * @param img the image to download from
 * @param offset the offset within the image
 * @param shape the shape of the region to update within the image
 * @param size the size of the downloaded data, in bytes
 * @param[out] data pointer to the buffer that will hold the downloaded data
 */
DVZ_EXPORT void dvz_download_image(
    DvzTransfers* transfers, DvzImages* img, //
    uvec3 offset, uvec3 shape, VkDeviceSize size, void* data);

/**
 * Copy part of a image to another.
 *
 * This function does not involve GPU-CPU data transfers.
 *
 * @param transfers the DvzTransfers pointer
 * @param src the source image
 * @param src_offset the offset within the source image
 * @param dst the target image
 * @param dst_offset the offset within the target image
 * @param shape the shape of the part of the image to copy
 * @param size the corresponding size of that part, in bytes
 */
DVZ_EXPORT void dvz_copy_image(
    DvzTransfers* transfers,          //
    DvzImages* src, uvec3 src_offset, //
    DvzImages* dst, uvec3 dst_offset, //
    uvec3 shape, VkDeviceSize size);



#endif
