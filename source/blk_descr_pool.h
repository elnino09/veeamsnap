#pragma once
#include "blk_descr_unify.h"

typedef struct blk_descr_pool_s
{
    struct list_head head;
    struct mutex lock;

    size_t blocks_cnt; //count of _pool_el_t

    volatile size_t total_cnt; ///count of blk_descr_mem_t
    volatile size_t take_cnt; // take count of blk_descr_mem_t;
}blk_descr_pool_t;

/*
 * dbg_kmalloc_huge( _POOL_EL_MAX_SIZE
 * 每个pool_el_t有8个页的空间
 */
typedef struct  pool_el_s
{
    struct list_head link;

    size_t used_cnt; // used blocks
    size_t capacity; // blocks array capacity

    blk_descr_unify_t blocks[0];  // 数据部分类型不是blk_descr_unify_t，可能是blk_descr_mem_t之类的
                                  // blk_descr_mem_t第一个成员就是blk_descr_unify_t结构体
}pool_el_t;

void blk_descr_pool_init( blk_descr_pool_t* pool, size_t available_blocks );

typedef void( *blk_descr_cleanup_t )(blk_descr_unify_t* blocks, size_t count);
void blk_descr_pool_done( blk_descr_pool_t* pool, blk_descr_cleanup_t blocks_cleanup );

typedef blk_descr_unify_t* (*blk_descr_alloc_t)(blk_descr_unify_t* blocks, size_t index, void* arg);
blk_descr_unify_t* blk_descr_pool_alloc( blk_descr_pool_t* pool, size_t blk_descr_size, blk_descr_alloc_t block_alloc, void* arg );

blk_descr_unify_t* blk_descr_pool_take( blk_descr_pool_t* pool, size_t blk_descr_size );

bool blk_descr_pool_check_halffill( blk_descr_pool_t* pool, sector_t empty_limit, sector_t* fill_status );
