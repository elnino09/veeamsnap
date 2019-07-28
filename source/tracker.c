#include "stdafx.h"
#include "tracker.h"
#include "blk_util.h"
#define SECTION "tracker   "
#include "log_format.h"

static container_sl_t trackers_container;

/*
 * 初始化全局container_sl_t变量trackers_container
 */
int tracker_init(void ){
    container_sl_init( &trackers_container, sizeof( tracker_t ) );
    return SUCCESS;
}

int tracker_done(void )
{
    int result = SUCCESS;

    result = tracker_remove_all();
    if (SUCCESS == result){
        if (SUCCESS != container_sl_done( &trackers_container ))
            log_err( "Failed to free up trackers container" );
        
        }
    else
        log_err("Failed to remove all tracking devices from tracking");

    return result;
}

/*
 * 通过tracker_queue_t和扇区关系找到tracker_t，
 * 1. 是同一个队列
 * 2. sector落在跟踪设备的扇区范围内
 */
int tracker_find_by_queue_and_sector( tracker_queue_t* queue, sector_t sector, tracker_t** ptracker )
{
    int result = -ENODATA;

    content_sl_t* content = NULL;
    tracker_t* tracker = NULL;
    CONTAINER_SL_FOREACH_BEGIN( trackers_container, content )
    {
        tracker = (tracker_t*)content;
        if ((queue == tracker->tracker_queue) &&
            (sector >= blk_dev_get_start_sect( tracker->target_dev )) &&
            (sector < (blk_dev_get_start_sect( tracker->target_dev ) + blk_dev_get_capacity( tracker->target_dev )))
        ){
            *ptracker = tracker;
            result = SUCCESS;
            break;
        }
    }
    CONTAINER_SL_FOREACH_END( trackers_container );

    return result;
}

/*
 * 通过tracker_queue_t和扇区关系找到tracker_t，
 * 1. 是同一个队列
 * 2. sector和跟踪设备的扇区范围有交集
 */
int tracker_find_intersection(tracker_queue_t* queue, sector_t b1, sector_t e1, tracker_t** ptracker)
{
    int result = -ENODATA;

    content_sl_t* content = NULL;
    tracker_t* tracker = NULL;
    CONTAINER_SL_FOREACH_BEGIN(trackers_container, content)
    {
        tracker = (tracker_t*)content;

        if (queue == tracker->tracker_queue)
        {
            bool have_intersection = false;
            sector_t b2 = blk_dev_get_start_sect(tracker->target_dev);
            sector_t e2 = blk_dev_get_start_sect(tracker->target_dev) + blk_dev_get_capacity(tracker->target_dev);

            if ((b1 >= b2) && (e1 <= e2))
                have_intersection = true;
            if ((b1 <= b2) && (e1 >= e2))
                have_intersection = true;
            if ((b1 <= b2) && (e1 > b2))
                have_intersection = true;
            if ((b1 < e2) && (e1 >= e2))
                have_intersection = true;

            if (have_intersection){
                if (ptracker != NULL)
                    *ptracker = tracker;

                result = SUCCESS;
                break;
            }
        }
    }
    CONTAINER_SL_FOREACH_END(trackers_container);

    return result;
}

int tracker_find_by_dev_id( dev_t dev_id, tracker_t** ptracker )
{
    int result = -ENODATA;

    content_sl_t* content = NULL;
    tracker_t* tracker = NULL;
    CONTAINER_SL_FOREACH_BEGIN( trackers_container, content )
//    read_lock( &trackers_container.lock );
//    if (!list_empty( &trackers_container.headList )){
//        struct list_head* _container_list_head;
//        list_for_each( _container_list_head, &trackers_container.headList ){
//            content = list_entry( _container_list_head, content_sl_t, link );
    {
        tracker = (tracker_t*)content;
        if (tracker->original_dev_id == dev_id){
            *ptracker = tracker;
            result =  SUCCESS;    //found!
            break;
        }
    }
//        }
//    }
//    read_unlock( &trackers_container.lock );


    CONTAINER_SL_FOREACH_END( trackers_container );

    return result;
}

/*
 * 遍历trackers_container，将所有已跟踪的tracker的设备号、容量等信息赋值给cbt_info_s结构
 * p_cbt_info为null的话就不用赋值了
 */
int tracker_enum_cbt_info( int max_count, struct cbt_info_s* p_cbt_info, int* p_count )
{
    int result = -ENODATA;
    int count = 0;
    content_sl_t* content = NULL;
    tracker_t* tracker = NULL;
    CONTAINER_SL_FOREACH_BEGIN( trackers_container, content )
    {
        tracker = (tracker_t*)content;

        if (count >= max_count){
            result = -ENOBUFS;
            break;    //don`t continue
        }

        if (p_cbt_info != NULL){
            p_cbt_info[count].dev_id.major = MAJOR(tracker->original_dev_id);
            p_cbt_info[count].dev_id.minor = MINOR(tracker->original_dev_id);

            if (tracker->cbt_map){
                p_cbt_info[count].cbt_map_size = tracker->cbt_map->map_size;
                p_cbt_info[count].snap_number = (unsigned char)tracker->cbt_map->snap_number_previous;
                veeam_uuid_copy((veeam_uuid_t*)(p_cbt_info[count].generationId), &tracker->cbt_map->generationId);
            }
            else{
                p_cbt_info[count].cbt_map_size = 0;
                p_cbt_info[count].snap_number = 0;
            }

            p_cbt_info[count].dev_capacity = sector_to_streamsize(blk_dev_get_capacity(tracker->target_dev));
        }
        //log_tr_dev_t("A device was found under tracking ", tracker->original_dev_id);
        ++count;
        result = SUCCESS;
    }
    CONTAINER_SL_FOREACH_END( trackers_container );
    *p_count = count;
    return result;
}

/*
 * 给tracker几个成员cbt_map（创建cbt_map对象）、cbt_block_size_degree、device_capacity赋值
*/
void tracker_cbt_start( tracker_t* tracker, unsigned long long snapshot_id, unsigned int cbt_block_size_degree, sector_t device_capacity )
{
    tracker_snapshot_id_set(tracker, snapshot_id);

    tracker->cbt_map = cbt_map_get_resource( cbt_map_create( (cbt_block_size_degree - SECTOR512_SHIFT), device_capacity ) );
    
    tracker->cbt_block_size_degree = cbt_block_size_degree;
    tracker->device_capacity = device_capacity;
}

/*
 * 构造tracker_t对象并赋值，并修改对应块设备的make_request_fn
 * 几个重要的结构：cbt_map_t、tracker_queue
 */
int tracker_create( unsigned long long snapshot_id, dev_t dev_id, unsigned int cbt_block_size_degree, tracker_t** ptracker )
{
    int result = SUCCESS;
    tracker_t* tracker = NULL;

    *ptracker = NULL;

    // 创建一个tracker_t对象并放在trackers_container的链表里
    tracker = (tracker_t*)container_sl_new( &trackers_container );
    if (NULL==tracker)
        return -ENOMEM;

    atomic_set( &tracker->is_captured, false);
    tracker->is_unfreezable = false;
    init_rwsem(&tracker->unfreezable_lock);

    tracker->original_dev_id = dev_id;

    // 获取block_device对象
    // 这里open块设备后没有close，close是在tracker_remove里做的
    result = blk_dev_open( tracker->original_dev_id, &tracker->target_dev );
    if (result != SUCCESS)
        return result;
    do{
        struct super_block* superblock = NULL;

        log_tr_format( "Create tracker for device [%d:%d]. Start 0x%llx sector, capacity 0x%llx sectors",
            MAJOR( tracker->original_dev_id ), MINOR( tracker->original_dev_id ), 
            (unsigned long long)blk_dev_get_start_sect( tracker->target_dev ),
            (unsigned long long)blk_dev_get_capacity( tracker->target_dev ) );

        tracker_cbt_start( tracker, snapshot_id, cbt_block_size_degree, blk_dev_get_capacity( tracker->target_dev ) );

        result = blk_freeze_bdev( tracker->original_dev_id, tracker->target_dev, &superblock );
        if (result != SUCCESS){
            tracker->is_unfreezable = true;
            break;
        }
        // bdev_get_queue函数获取与块设备相关的请求队列q
        result = tracker_queue_ref( bdev_get_queue( tracker->target_dev ), &tracker->tracker_queue );
        superblock = blk_thaw_bdev( tracker->original_dev_id, tracker->target_dev, superblock );

    }while(false);

    if (SUCCESS ==result){
        *ptracker = tracker;
    }else{
        int remove_status = SUCCESS;

        log_err_dev_t( "Failed to create tracker for device ", tracker->original_dev_id );

        remove_status = tracker_remove(tracker);
        if ((SUCCESS == remove_status) || (-ENODEV == remove_status))
            tracker = NULL;
        else
            log_err_d( "Failed to perfrom tracker cleanup. errno=", (0 - remove_status) );
        }

    return result;
}

/*
 * 释放tracker_queue、cbt_map_t结构体空间
 */
int _tracker_remove( tracker_t* tracker )
{
    int result = SUCCESS;

    if (NULL != tracker->target_dev){

        struct super_block* superblock = NULL;
   
        if (tracker->is_unfreezable)
            down_write(&tracker->unfreezable_lock);
        else
            result = blk_freeze_bdev(tracker->original_dev_id, tracker->target_dev, &superblock);

        if (NULL != tracker->tracker_queue){
            tracker_queue_unref( tracker->tracker_queue );
            tracker->tracker_queue = NULL;
        }
        if (tracker->is_unfreezable)
            up_write(&tracker->unfreezable_lock);
        else
            superblock = blk_thaw_bdev(tracker->original_dev_id, tracker->target_dev, superblock);

        blk_dev_close( tracker->target_dev );

        tracker->target_dev = NULL;
    }else
        result=-ENODEV;

    if (NULL != tracker->cbt_map){
        cbt_map_put_resource( tracker->cbt_map );
        tracker->cbt_map = NULL;
    }

    return result;
}

/*
 * 释放tracker_queue、cbt_map_t结构体空间
 * 释放tracker_t空间
 */
int tracker_remove(tracker_t* tracker)
{
    int result = _tracker_remove( tracker );

    container_sl_free( &tracker->content );

    return result;
}

/*
 * 移除所有跟踪的设备
 * 将 trackers_container 的链表清空
*/
int tracker_remove_all(void )
{
    int result = SUCCESS;
    int status;
    content_sl_t* content = NULL;

    log_tr("Removing all devices from tracking");

    while (NULL != (content = container_sl_get_first( &trackers_container ))){
        tracker_t* tracker = (tracker_t*)content;

        status = _tracker_remove( tracker );
        if (status != SUCCESS)
            log_err_format( "Failed to remove device [%d:%d] from tracking. errno=%d",
                MAJOR( tracker->original_dev_id ), MINOR( tracker->original_dev_id ), 0 - status );

        content_sl_free( content );
        content = NULL;
    }

    return result;
}

/*
 * 将cbtmap的write_map中的cbt块对应的byte设置为当前的snap_number
 */
int tracker_cbt_bitmap_set( tracker_t* tracker, sector_t sector, sector_t sector_cnt )
{
    int res = SUCCESS;
    if (tracker->device_capacity == blk_dev_get_capacity( tracker->target_dev )){
        if (tracker->cbt_map)
            res = cbt_map_set( tracker->cbt_map, sector, sector_cnt );
    }
    else{
        log_warn( "Device resize detected" );
        res = -EINVAL;
    }

    if (SUCCESS != res){ //cbt corrupt
        if (tracker->cbt_map){
            log_warn( "CBT fault detected" );
            tracker->cbt_map->active = false;
        }
    }
    return res;
}

bool tracker_cbt_bitmap_lock( tracker_t* tracker )
{
    bool result = false;
    if (tracker->cbt_map){
        cbt_map_read_lock( tracker->cbt_map );

        if (tracker->cbt_map->active){
            result = true;
        }
        else
            cbt_map_read_unlock( tracker->cbt_map );
    }
    return result;
}

void tracker_cbt_bitmap_unlock( tracker_t* tracker )
{
    if (tracker->cbt_map)
        cbt_map_read_unlock( tracker->cbt_map );
}

/*
 * 调用创建defer_io_t对象并创建dio线程运行
 */
int _tracker_capture_snapshot( tracker_t* tracker )
{
    int result = SUCCESS;
    defer_io_t* defer_io = NULL;

    result = defer_io_create( tracker->original_dev_id, tracker->target_dev, &defer_io );
    if (result != SUCCESS){
        log_err( "Failed to create defer IO processor" );
    }else{
        tracker->defer_io = defer_io_get_resource( defer_io );

        atomic_set( &tracker->is_captured, true );

        if (tracker->cbt_map != NULL){
            cbt_map_write_lock( tracker->cbt_map );
            cbt_map_switch( tracker->cbt_map );
            cbt_map_write_unlock( tracker->cbt_map );

            log_tr_format( "Snapshot captured for device [%d:%d]. New snap number %ld",
                MAJOR( tracker->original_dev_id ), MINOR( tracker->original_dev_id ), tracker->cbt_map->snap_number_active );
    }
    }

    return result;

}

/*
 * 创建快照并检查快照是否损坏
 */
int tracker_capture_snapshot( snapshot_t* snapshot )
{
    int result = SUCCESS;
    int inx = 0;

    for (inx = 0; inx < snapshot->dev_id_set_size; ++inx){
        struct super_block* superblock = NULL;
        tracker_t* tracker = NULL;
        dev_t dev_id = snapshot->dev_id_set[inx];

        result = tracker_find_by_dev_id( dev_id, &tracker );
        if (result != SUCCESS){
            log_err_dev_t( "Unable to capture snapshot: cannot find device ", dev_id );
            break;
        }

        // 如果该块设备不支持freeze，那么通过信号量unfreezable_lock加锁
        if (tracker->is_unfreezable)
            down_write(&tracker->unfreezable_lock);
        else
            blk_freeze_bdev(tracker->original_dev_id, tracker->target_dev, &superblock);
        {
            result = _tracker_capture_snapshot( tracker );
            if (result != SUCCESS)
                log_err_dev_t( "Failed to capture snapshot for device ", dev_id );
        }
        if (tracker->is_unfreezable)
            up_write(&tracker->unfreezable_lock);
        else
            superblock = blk_thaw_bdev(tracker->original_dev_id, tracker->target_dev, superblock);
    }
    if (result != SUCCESS)
        return result;

    for (inx = 0; inx < snapshot->dev_id_set_size; ++inx){
        tracker_t* p_tracker = NULL;
        dev_t dev_id = snapshot->dev_id_set[inx];

        result = tracker_find_by_dev_id( dev_id, &p_tracker );

        if (snapstore_device_is_corrupted( p_tracker->defer_io->snapstore_device )){
            log_err_format( "Unable to freeze devices [%d:%d]: snapshot data is corrupted", dev_id );
            result = -EDEADLK;
            break;
        }
    }

    if (result != SUCCESS){
        int status;
        log_err_d( "Failed to capture snapshot. errno=", result );

        status = tracker_release_snapshot( snapshot );
        if (status != SUCCESS)
            log_err_d( "Failed to perfrom snapshot cleanup. errno= ", status );
    }
    return result;
}


int _tracker_release_snapshot( tracker_t* tracker )
{
    int result = SUCCESS;
    struct super_block* superblock = NULL;
    defer_io_t* defer_io = tracker->defer_io;


    if (tracker->is_unfreezable)
        down_write(&tracker->unfreezable_lock);
    else
        result = blk_freeze_bdev(tracker->original_dev_id, tracker->target_dev, &superblock);
    {
        //clear freeze flag
        atomic_set(&tracker->is_captured, false);

        tracker->defer_io = NULL;
    }
    if (tracker->is_unfreezable)
        up_write(&tracker->unfreezable_lock);
    else
        superblock = blk_thaw_bdev(tracker->original_dev_id, tracker->target_dev, superblock);

    defer_io_stop(defer_io);
    defer_io_put_resource(defer_io);

    return result;
}


int tracker_release_snapshot( snapshot_t* snapshot )
{
    int result = SUCCESS;
    int inx = 0;
    log_tr_format( "Release snapshot [0x%llx]", snapshot->id );

    for (; inx < snapshot->dev_id_set_size; ++inx){
        int status;
        tracker_t* p_tracker = NULL;
        dev_t dev = snapshot->dev_id_set[inx];

        status = tracker_find_by_dev_id( dev, &p_tracker );
        if (status == SUCCESS){
            status = _tracker_release_snapshot( p_tracker );
            if (status != SUCCESS){
                log_err_dev_t( "Failed to release snapshot for device ", dev );
                result = status;
                break;
            }
        }
        else
            log_err_dev_t( "Unable to release snapshot: cannot find tracker for device ", dev );
    }

    return result;
}


void tracker_print_state( void )
{
    size_t sz;
    tracker_t** trackers;
    int tracksers_cnt = 0;

    tracksers_cnt = container_sl_length( &trackers_container );
    sz = tracksers_cnt * sizeof( tracker_t* );
    trackers = dbg_kzalloc( sz, GFP_KERNEL );
    if (trackers == NULL){
        log_err_sz( "Failed to allocate buffer for trackers. Size=", sz );
        return;
    }

    do{
        size_t inx = 0;
        content_sl_t* pContent = NULL;
        CONTAINER_SL_FOREACH_BEGIN( trackers_container, pContent )
        {
            tracker_t* tracker = (tracker_t*)pContent;

            trackers[inx] = tracker;
            inx++;
            if (inx >= tracksers_cnt)
                break;
        }
        CONTAINER_SL_FOREACH_END( trackers_container );

        log_tr( "" );
        log_tr( "Trackers state:" );
        for (inx = 0; inx < tracksers_cnt; ++inx){

            if (NULL != trackers[inx]){
                log_tr_dev_t( "original device ", trackers[inx]->original_dev_id );
                if (trackers[inx]->defer_io)
                    defer_io_print_state( trackers[inx]->defer_io );
            }
        }
    } while (false);
    dbg_kfree( trackers );
}

void tracker_snapshot_id_set(tracker_t* tracker, unsigned long long snapshot_id)
{
    tracker->snapshot_id = snapshot_id;
}

unsigned long long tracker_snapshot_id_get(tracker_t* tracker)
{
    return tracker->snapshot_id;
}
