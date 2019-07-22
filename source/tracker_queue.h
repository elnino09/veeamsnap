#pragma once
#include "container_spinlocking.h"

typedef struct _tracker_queue_s
{
    content_sl_t content;

    struct request_queue*    original_queue;  // 内核就提供一种队列的机制把这些I/O请求添加到队列中(即：请求队列)，在驱动中用request_queue结构体描述
    make_request_fn*        original_make_request_fn;  // 将一个新请求插入请求队列时调用的方法，块设备的io调度程序主要是在该函数内完成

    atomic_t                atomic_ref_count;

}tracker_queue_t;

int tracker_queue_init(void );
int tracker_queue_done(void );

int tracker_queue_ref( struct request_queue* queue,    tracker_queue_t** ptracker_queue );
void tracker_queue_unref( tracker_queue_t* ptracker_queue );
int tracker_queue_find(    struct request_queue* queue, tracker_queue_t** ptracker_queue);
