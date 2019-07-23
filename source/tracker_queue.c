/*
 * 这个tracker_queue_t记录的东西比较少，仅记录了原设备的请求队列，以及原设备的原始make_request_fn函数
 */

#include "stdafx.h"
#include "container_spinlocking.h"
#include "tracker_queue.h"

#define SECTION "tracker   "

#if  LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)

#ifdef HAVE_MAKE_REQUEST_INT
int tracking_make_request(struct request_queue *q, struct bio *bio);
#else
void tracking_make_request(struct request_queue *q, struct bio *bio);
#endif

#else
blk_qc_t tracking_make_request( struct request_queue *q, struct bio *bio );
#endif


container_sl_t tracker_queue_container;

int tracker_queue_init(void )
{
    container_sl_init(&tracker_queue_container, sizeof(tracker_queue_t));
    return SUCCESS;
}

int tracker_queue_done(void )
{
    int result = container_sl_done( &tracker_queue_container );
    if (SUCCESS != result)
        log_err( "Failed to free up tracker queue container");
    return result;
}

// 将请求队列和追踪队列tracker_queue_t关联
// find or create new tracker queue
// 将queue里的 make_request_fn 更换为 tracking_make_request ，
// 同时用 ptracker_queue->original_make_request_fn 记录 queue 原来的 make_request_fn
int tracker_queue_ref(    struct request_queue* queue, tracker_queue_t** ptracker_queue )
{
    int find_result = SUCCESS;
    tracker_queue_t* tr_q = NULL;

    // 如果在 tracker_queue_container 中找到对应的 tracker_queue 就返回
    find_result = tracker_queue_find(queue, &tr_q);
    if (SUCCESS == find_result){
        log_tr("Tracker queue already exists");

        *ptracker_queue = tr_q;
        atomic_inc( &tr_q->atomic_ref_count );

        return find_result;
    }

    // 返回不为0或ENODATA，就是出错了
    if (-ENODATA != find_result){
        log_tr_d( "Cannot to find tracker queue. errno=", find_result );
        return find_result;
    }
    // 找不到tracker_queue_t时新生成一个
    log_tr("New tracker queue create" );

    tr_q = (tracker_queue_t*)container_sl_new(&tracker_queue_container);
    if (NULL==tr_q)
        return -ENOMEM;

    // 原子操作里修改 queue->make_request_fn
    atomic_set( &tr_q->atomic_ref_count, 0 );

    tr_q->original_make_request_fn = queue->make_request_fn;
    queue->make_request_fn = tracking_make_request;

    tr_q->original_queue = queue;

    *ptracker_queue = tr_q;
    atomic_inc( &tr_q->atomic_ref_count );

    log_tr("New tracker queue was created");

    return SUCCESS;
}

/*
 * 恢复设备原来的make_request_fn，释放tracker_queue_t结构体空间
 */
void tracker_queue_unref( tracker_queue_t* tracker_queue )
{
    // atomic_dec_and_test 该函数对原子类型的变量v原子地减1，并判断结果是否为0，如果为0，返回真，否则返回假。
    // 这里如果返回假那就是说这个队列还被占用
    if ( atomic_dec_and_test( &tracker_queue->atomic_ref_count ) ){
        // 恢复设备原来的make_request_fn
        if (NULL != tracker_queue->original_make_request_fn){
            tracker_queue->original_queue->make_request_fn = tracker_queue->original_make_request_fn;
            tracker_queue->original_make_request_fn = NULL;
        }

        container_sl_free( &tracker_queue->content );

        log_tr("Tracker queue freed");
    }else
        log_tr("Tracker queue is in use");
}

// 根据disk关联的请求队列找到tracker_queue_t对象
int tracker_queue_find( struct request_queue* queue, tracker_queue_t** ptracker_queue )
{
    int result = -ENODATA;
    content_sl_t* pContent = NULL;
    tracker_queue_t* tr_q = NULL;
    CONTAINER_SL_FOREACH_BEGIN( tracker_queue_container, pContent )
    {
        tr_q = (tracker_queue_t*)pContent;
        if (tr_q->original_queue == queue){
            *ptracker_queue = tr_q;

            result = SUCCESS;    //don`t continue
            break;
        }
    }CONTAINER_SL_FOREACH_END( tracker_queue_container );

    return result;
}

