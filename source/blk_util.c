#include "stdafx.h"
#include "container.h"
#include "queue_spinlocking.h"
#include "blk_util.h"

#define SECTION "blk       "
#include "log_format.h"

/*
打开块设备，根据dev_id打开对应的块设备，p_blk_dev保存打开的块设备
*/
int blk_dev_open( dev_t dev_id, struct block_device** p_blk_dev )
{
    int result = SUCCESS;
    struct block_device* blk_dev;
    int refCount;

    blk_dev = bdget( dev_id );  // 根据块设备文件对应的设备标识符，在bdev文件系统中查找或新建一个索引节点、块设备描述符，返回块设备描述符的地址
    if (NULL == blk_dev){
        log_err_format( "Unable to open device [%d:%d]: bdget returned NULL", MAJOR( dev_id ), MINOR( dev_id ) );
        return -ENODEV;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
    refCount = blkdev_get( blk_dev, FMODE_READ | FMODE_WRITE );
#else
    // 打开块设备
    // Open bdev with mode. If mode includes FMODE_EXCL, bdev is open with exclusive access.
    // Specifying FMODE_EXCL with NULL holder is invalid. Exclusive opens may nest for the same holder.
    // On success, the reference count of bdev is unchanged. On failure, bdev is put.
    refCount = blkdev_get( blk_dev, FMODE_READ | FMODE_WRITE, NULL );  // FMODE_WRITE?
#endif
    if (refCount < 0){
        log_err_format( "Unable to open device [%d,%d]: blkdev_get returned errno=%d", MAJOR( dev_id ), MINOR( dev_id ), 0 - refCount );
        result = refCount;
    }

    if (result == SUCCESS)
        *p_blk_dev = blk_dev;
    return result;
}

void blk_dev_close( struct block_device* blk_dev )
{
    /*
    >> * blkdev_get() is extended to include exclusive access management.
    >>   @holder argument is added and, if is @FMODE_EXCL specified, it will
    >>   gain exclusive access atomically w.r.t. other exclusive accesses.
    >>
    >> * blkdev_put() is similarly extended.  It now takes @mode argument and
    >>   if @FMODE_EXCL is set, it releases an exclusive access.  Also, when
    >>   the last exclusive claim is released, the holder/slave symlinks are
    >>   removed automatically.
    */
    blkdev_put( blk_dev, FMODE_READ );
}

int _blk_dev_get_info( struct block_device* blk_dev, blk_dev_info_t* pdev_info )
{
    sector_t SectorStart;
    sector_t SectorsCapacity;

    if (blk_dev->bd_part)
        SectorsCapacity = blk_dev->bd_part->nr_sects;
    else if (blk_dev->bd_disk)
        SectorsCapacity = get_capacity( blk_dev->bd_disk );
    else{
        return -EINVAL;
    }

    SectorStart = get_start_sect( blk_dev );

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
    if (blk_dev->bd_disk){
        pdev_info->physical_block_size = blk_dev->bd_disk->queue->limits.physical_block_size;
        pdev_info->logical_block_size = blk_dev->bd_disk->queue->limits.logical_block_size;
        pdev_info->io_min = blk_dev->bd_disk->queue->limits.io_min;
    }
    else{
        pdev_info->physical_block_size = SECTOR512;
        pdev_info->logical_block_size = SECTOR512;
        pdev_info->io_min = SECTOR512;
    }
#else
    pdev_info->physical_block_size = blk_dev->bd_queue->limits.physical_block_size;
    pdev_info->logical_block_size = blk_dev->bd_queue->limits.logical_block_size;
    pdev_info->io_min = blk_dev->bd_queue->limits.io_min;
#endif

    pdev_info->blk_size = blk_dev_get_block_size( blk_dev );
    pdev_info->start_sect = SectorStart;
    pdev_info->count_sect = SectorsCapacity;
    return SUCCESS;
}

int blk_dev_get_info( dev_t dev_id, blk_dev_info_t* pdev_info )
{
    int result = SUCCESS;
    struct block_device* blk_dev;

    result = blk_dev_open( dev_id, &blk_dev );
    if (result != SUCCESS){
        log_err_dev_t( "Failed to open device ", dev_id );
        return result;
    }
    result = _blk_dev_get_info( blk_dev, pdev_info );
    if (result != SUCCESS){
        log_err_dev_t( "Failed to identify block device ", dev_id );
    }

    blk_dev_close( blk_dev );

    return result;
}

/*
 * 尝试调用freeze_bdev来freeze设备
 */
int blk_freeze_bdev( dev_t dev_id, struct block_device* device, struct super_block** psuperblock )
{
    struct super_block* superblock;

    if (device->bd_super == NULL){
        log_warn_format("Unable to freeze device [%d:%d]: no superblock was found", MAJOR(dev_id), MINOR(dev_id));
        return SUCCESS;
    }

    superblock = freeze_bdev(device);
    if (IS_ERR_OR_NULL(superblock)){
        int errcode;
        log_err_dev_t("Failed to freeze device ", dev_id);

        if (NULL == superblock)
            errcode = -ENODEV;
        else{
            errcode = PTR_ERR(superblock);
            log_err_d("Error code: ", errcode);
        }
        return errcode;
    }

    log_tr_format("Device [%d:%d] was frozen", MAJOR(dev_id), MINOR(dev_id));
    *psuperblock = superblock;

    return SUCCESS;
}

/*
 * 尝试调用thaw_bdev来解冻设备
 */
struct super_block* blk_thaw_bdev( dev_t dev_id, struct block_device* device, struct super_block* superblock )
{
    if (superblock != NULL){
        int result = thaw_bdev( device, superblock );
        if (result == SUCCESS)
            log_tr_format( "Device [%d:%d] was unfrozen", MAJOR( dev_id ), MINOR( dev_id ) );
        else
            log_err_dev_t( "Failed to unfreeze device ", dev_id );

        superblock = NULL;
    }
    return superblock;
}
