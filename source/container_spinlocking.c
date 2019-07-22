#include "stdafx.h"
#include "container_spinlocking.h"

#define SECTION "container "


static atomic_t g_container_sl_alloc_cnt = ATOMIC_INIT( 0 );

int container_sl_alloc_counter( void )
{
    return atomic_read( &g_container_sl_alloc_cnt );
}

void container_sl_init( container_sl_t* pContainer, int content_size )
{
    INIT_LIST_HEAD( &pContainer->headList );

    rwlock_init( &pContainer->lock );

    pContainer->content_size = content_size;

    atomic_set( &pContainer->cnt, 0 );
}

// 检查container_sl_t结构体的链表是否已请款，并将content_size也置为0，这样后续不可能再构造content_t结构体了
int container_sl_done( container_sl_t* pContainer )
{
    int cnt;

    if (pContainer->content_size != 0){
        cnt = atomic_read( &pContainer->cnt );
        if ((cnt != 0) || !list_empty( &pContainer->headList )){
            log_err_d( "CRITICAL ERROR: Container is not empty. length=", cnt );
            return -EBUSY;
        }

        pContainer->content_size = 0;
    }
    return SUCCESS;
}

void container_sl_print_state( void )
{
    log_tr( "" );
    log_tr( "Container SL state:" );
    log_tr_d( "Now allocated contents ", container_sl_alloc_counter( ) );
}

content_sl_t* container_sl_new( container_sl_t* pContainer )
{
    content_sl_t* pCnt = content_sl_new( pContainer );
    if (NULL != pCnt)
        container_sl_push_back( pContainer, pCnt );
    return pCnt;
}

/*
 * 将指定的content_sl_t从container_sl_t链表中删掉
 */
void _container_sl_free( container_sl_t* pContainer, content_sl_t* pCnt )
{
    if (pCnt != NULL){
        write_lock( &pContainer->lock );
        {
            // 从container_sl_t链表中删掉
            list_del( &pCnt->link );
        }
        write_unlock( &pContainer->lock );

        atomic_dec( &pContainer->cnt );

        // 也释放pCnt，content_sl_free用的是memset( pCnt, 0xFF, pContainer->content_size );
        // 考虑到初始化时用的是tracker_queue_t等，这里把整个tracker_queue_t内容给memset了
        content_sl_free( pCnt );
    }
}

/*
 * 将指定的content_sl_t从container_sl_t链表中删掉
 */
void container_sl_free( content_sl_t* pCnt )
{
    container_sl_t* pContainer = pCnt->container;

    _container_sl_free(pContainer, pCnt);
}


int container_sl_length( container_sl_t* pContainer )
{
    return atomic_read( &pContainer->cnt );
}

bool container_sl_empty( container_sl_t* pContainer )
{
    return (0 == container_sl_length( pContainer ));
}

size_t container_sl_push_back( container_sl_t* pContainer, content_sl_t* pCnt )
{
    size_t index = 0;

    if (NULL != pCnt){
        INIT_LIST_HEAD( &pCnt->link );

        write_lock( &pContainer->lock );
        {
            list_add_tail( &pCnt->link, &pContainer->headList );
            index = atomic_inc_return( &pContainer->cnt );
        }
        write_unlock( &pContainer->lock );
    }

    return index;
}
void container_sl_get( content_sl_t* pCnt )
{
    container_sl_t* pContainer = pCnt->container;
    write_lock( &pContainer->lock );
    do{
        list_del( &pCnt->link );
        atomic_dec( &pContainer->cnt );
    } while (false);
    write_unlock( &pContainer->lock );
}


/*
 * 获取trackers_container中的第一个content_sl_t节点，并把该节点从trackers_container链表中删掉
 */
content_sl_t* container_sl_get_first( container_sl_t* pContainer )
{
    content_sl_t* pCnt = NULL;

    write_lock( &pContainer->lock );
    {
        if (!list_empty( &pContainer->headList )){
            pCnt = list_entry( pContainer->headList.next, content_sl_t, link );

            list_del( &pCnt->link );
            atomic_dec( &pContainer->cnt );
        }
    }
    write_unlock( &pContainer->lock );

    return pCnt;
}


content_sl_t* content_sl_new_opt( container_sl_t* pContainer, gfp_t gfp_opt )
{
    content_sl_t* pCnt = dbg_kmalloc( pContainer->content_size, gfp_opt );

    if (pCnt){
        atomic_inc( &g_container_sl_alloc_cnt );

        memset( pCnt, 0, pContainer->content_size );

        pCnt->container = pContainer;
    }
    return pCnt;
}

content_sl_t* content_sl_new( container_sl_t* pContainer )
{
    return content_sl_new_opt( pContainer, GFP_KERNEL );
}

/*
 * 释放content_sl_t节点空间，其实这里释放的空间要比content_sl_t大，因为通常content_sl_t是上层结构体的一个成员，
 * 上层结构体的大小记录在content_sl_t->container->content_size里
 */
void content_sl_free( content_sl_t* pCnt )
{
    if (pCnt){
        container_sl_t* pContainer = pCnt->container;

        memset( pCnt, 0xFF, pContainer->content_size );

        atomic_dec( &g_container_sl_alloc_cnt );

        dbg_kfree( pCnt );
    }
}

content_sl_t* container_sl_at( container_sl_t* pContainer, size_t inx )
{
    size_t count = 0;
    content_sl_t* pResult = NULL;
    content_sl_t* content = NULL;

    read_lock( &pContainer->lock );
    if (!list_empty( &pContainer->headList )){
        struct list_head* _container_list_head;
        list_for_each( _container_list_head, &pContainer->headList ){

            content = list_entry( _container_list_head, content_sl_t, link );
            if (inx == count){
                pResult = content;
                break;
            }
            ++count;
        }
    }
    read_unlock( &pContainer->lock );

    return pResult;
}
