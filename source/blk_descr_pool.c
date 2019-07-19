#include "stdafx.h"
#include "blk_descr_pool.h"

#define SECTION "blk_descr "
#include "log_format.h"


#define _POOL_EL_MAX_SIZE (8*PAGE_SIZE)

static pool_el_t* pool_el_alloc( size_t blk_descr_size )
{
    size_t el_size;
    pool_el_t* el = (pool_el_t*)dbg_kmalloc_huge( _POOL_EL_MAX_SIZE, PAGE_SIZE, GFP_NOIO, &el_size );
    if (NULL == el)
        return NULL;

    el->capacity = (el_size - sizeof( pool_el_t )) / blk_descr_size;
    el->used_cnt = 0;

    INIT_LIST_HEAD( &el->link );

    return el;
}

static void _pool_el_free( pool_el_t* el )
{
    if (el != NULL)
        dbg_kfree( el );
}

void blk_descr_pool_init( blk_descr_pool_t* pool, size_t available_blocks )
{
    mutex_init(&pool->lock);

    INIT_LIST_HEAD( &pool->head );

    pool->blocks_cnt = 0;

    pool->total_cnt = available_blocks;
    pool->take_cnt = 0;
}

void blk_descr_pool_done( blk_descr_pool_t* pool, blk_descr_cleanup_t blocks_cleanup )
{
    mutex_lock( &pool->lock );
    while (!list_empty( &pool->head ))
    {
        pool_el_t* el = list_entry( pool->head.next, pool_el_t, link );
        if (el == NULL)
            break;

        list_del( &el->link );
        --pool->blocks_cnt;

        pool->total_cnt -= el->used_cnt;

        blocks_cleanup( el->blocks, el->used_cnt );

        _pool_el_free( el );

    }
    mutex_unlock(&pool->lock);
}

blk_descr_unify_t* blk_descr_pool_alloc( blk_descr_pool_t* pool, size_t blk_descr_size, blk_descr_alloc_t block_alloc, void* arg )
{
    blk_descr_unify_t* blk_descr = NULL;

    mutex_lock(&pool->lock);
    do{
        pool_el_t* el = NULL;

        if (!list_empty( &pool->head )){
            el = list_entry( pool->head.prev, pool_el_t, link );
            if (el->used_cnt == el->capacity)
                el = NULL;
        }

        if (el == NULL){
            el = pool_el_alloc( blk_descr_size );
            if (NULL == el)
                break;

            list_add_tail( &el->link, &pool->head );

            ++pool->blocks_cnt;
        }

        blk_descr = block_alloc( el->blocks, el->used_cnt, arg );

        ++el->used_cnt;
        ++pool->total_cnt;

    } while (false);
    mutex_unlock(&pool->lock);

    return blk_descr;
}


#define _FOREACH_EL_BEGIN( pool, el )  \
if (!list_empty( &(pool)->head )){ \
    struct list_head* _list_head; \
    list_for_each( _list_head, &(pool)->head ){ \
        el = list_entry( _list_head, pool_el_t, link );

#define _FOREACH_EL_END( ) \
    } \
}

static blk_descr_unify_t* __blk_descr_pool_at( blk_descr_pool_t* pool, size_t blk_descr_size, size_t index )
{
    void* bkl_descr = NULL;
    size_t curr_inx = 0;
    pool_el_t* el;

    _FOREACH_EL_BEGIN( pool, el )
    {
        if ((index >= curr_inx) && (index < (curr_inx + el->used_cnt))){
            bkl_descr = (void*)(el->blocks) + (index - curr_inx) * blk_descr_size;
            break;
        }
        curr_inx += el->used_cnt;
    }
    _FOREACH_EL_END( );

    return (blk_descr_unify_t*)bkl_descr;
}

blk_descr_unify_t* blk_descr_pool_take( blk_descr_pool_t* pool, size_t blk_descr_size )
{
    blk_descr_unify_t* result = NULL;
    mutex_lock(&pool->lock);
    do{
        if (pool->take_cnt >= pool->total_cnt){
            log_err_format("Unable to get block descriptor: not enough descriptors. Already took %ld, total %ld", pool->take_cnt, pool->total_cnt);
            break;
        }

        result = __blk_descr_pool_at(pool, blk_descr_size, pool->take_cnt);
        if (result == NULL){
            log_err_format("Unable to get block descriptor: not enough descriptors. Already took %ld, total %ld", pool->take_cnt, pool->total_cnt);
            break;
        }

        ++pool->take_cnt;
    } while (false);
    mutex_unlock(&pool->lock);
    return result;
}


bool blk_descr_pool_check_halffill( blk_descr_pool_t* pool, sector_t empty_limit, sector_t* fill_status )
{
    size_t empty_blocks = (pool->total_cnt - pool->take_cnt);

    *fill_status = (sector_t)(pool->take_cnt) << SNAPSTORE_BLK_SHIFT;

    return (empty_blocks < (size_t)(empty_limit >> SNAPSTORE_BLK_SHIFT));
}