#pragma once

#include "page_array.h"
#include "sector.h"

typedef struct blk_dev_info_s
{
    size_t blk_size;
    sector_t start_sect;
    sector_t count_sect;

    unsigned int io_min;
    unsigned int physical_block_size;
    unsigned short logical_block_size;

}blk_dev_info_t;


int blk_dev_open( dev_t dev_id, struct block_device** p_blk_dev );

void blk_dev_close( struct block_device* blk_dev );


int blk_dev_get_info( dev_t dev_id, blk_dev_info_t* pdev_info );
int _blk_dev_get_info( struct block_device* blk_dev, blk_dev_info_t* pdev_info );

int blk_freeze_bdev( dev_t dev_id, struct block_device* device, struct super_block** psuperblock );
struct super_block* blk_thaw_bdev( dev_t dev_id, struct block_device* device, struct super_block* superblock );

static __inline sector_t blk_dev_get_capacity( struct block_device* blk_dev )
{
    /* 该分区的扇区个数，也就是分区容量 */
    return blk_dev->bd_part->nr_sects;
};

// 返回该分区的起始扇区号
static __inline sector_t blk_dev_get_start_sect( struct block_device* blk_dev )
{
    /* block_device结构代表了内核中的一个块设备。它可以表示整个磁盘或一个特定的分区。
     * 当这个结构代表一个分区时，它的bd_contains成员指向包含这个分区的设备，bd_part成员指向设备的分区结构。
     * 当这个结构代表一个块设备时，bd_disk成员指向设备的gendisk结构。
     */
     // gendisk是对通用磁盘的一个描述，与真正的底层物理设备相关联。
     // struct hd_struct *    bd_part; /* 指向分区指针，对于gendisk，指向内置的分区0 */
    return blk_dev->bd_part->start_sect;
};

static __inline size_t blk_dev_get_block_size( struct block_device* blk_dev ){
    return (size_t)block_size( blk_dev );
}

static __inline void blk_bio_end( struct bio *bio, int err )
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
    bio_endio( bio, err );
#else

#ifndef BLK_STS_OK//#if LINUX_VERSION_CODE < KERNEL_VERSION( 4, 13, 0 )
    bio->bi_error = err;
#else
    if (err == SUCCESS)
        bio->bi_status = BLK_STS_OK;
    else
        bio->bi_status = BLK_STS_IOERR;
#endif
    bio_endio( bio );
#endif
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)

#define bio_vec_page(bv)    bv->bv_page
#define bio_vec_offset(bv)  bv->bv_offset
#define bio_vec_len(bv)     bv->bv_len
#define bio_vec_buffer(bv)  (page_address( bv->bv_page ) + bv->bv_offset)
#define bio_vec_sectors(bv) (bv->bv_len>>SECTOR512_SHIFT)

#define bio_bi_sector(bio)  bio->bi_sector  // 我们想在块设备的第几个扇区上进行io操作（起始扇区），此处扇区大小是按512计算的
#define bio_bi_size(bio)    bio->bi_size    // 需要传送的字节数

#else

#define bio_vec_page(bv)    bv.bv_page
#define bio_vec_offset(bv)  bv.bv_offset
#define bio_vec_len(bv)     bv.bv_len
#define bio_vec_buffer(bv)  (page_address( bv.bv_page ) + bv.bv_offset)
#define bio_vec_sectors(bv) (bv.bv_len>>SECTOR512_SHIFT)

#define bio_bi_sector(bio)  bio->bi_iter.bi_sector
#define bio_bi_size(bio)    bio->bi_iter.bi_size

#endif


static inline
sector_t blk_bio_io_vec_sectors( struct bio* bio )
{
    sector_t sect_cnt = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)
    struct bio_vec* bvec;
    unsigned short iter;
#else
    struct bio_vec bvec;
    struct bvec_iter iter;
#endif
    bio_for_each_segment( bvec, bio, iter ){
        sect_cnt += ( bio_vec_len( bvec ) >> SECTOR512_SHIFT );
    }
    return sect_cnt;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION( 4, 18, 0 )
static inline
struct bio_set* blk_bioset_create(unsigned int front_pad)
{
#ifdef OS_RELEASE_SUSE
#if LINUX_VERSION_CODE < KERNEL_VERSION( 4, 12, 14 )
    return bioset_create(64, front_pad);
#else
    return bioset_create(64, front_pad, BIOSET_NEED_BVECS | BIOSET_NEED_RESCUER);
#endif
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION( 4, 13, 0 )
    return bioset_create(64, front_pad);
#else
    return bioset_create(64, front_pad, BIOSET_NEED_BVECS | BIOSET_NEED_RESCUER);
#endif
#endif
}
#endif