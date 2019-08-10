#pragma once

#include "page_array.h"
#include "shared_resource.h"
#include "uuid_util.h"

typedef struct cbt_map_s
{
    shared_resource_t sharing_header;

    spinlock_t locker;

    size_t   sect_in_block_degree;  // 每个cbt块里有2**sect_in_block_degree个扇区
    size_t   map_size;              // 该块设备有几个cbt块

    page_array_t*  read_map;   // 保存cbt块的快照信息，每个cbt块用1个byte表示，记录的是snapnumber
    page_array_t*  write_map;  // 保存cbt块的快照信息，每个cbt块用1个byte表示，记录的是snapnumber

    volatile unsigned long snap_number_active;    // 当前快照顺序号
    volatile unsigned long snap_number_previous;  // 上一个快照顺序号
    veeam_uuid_t generationId;

    volatile bool active;

    struct rw_semaphore rw_lock;
}cbt_map_t;

cbt_map_t* cbt_map_create( unsigned int cbt_sect_in_block_degree, sector_t blk_dev_sect_count );
void cbt_map_destroy( cbt_map_t* cbt_map );

void cbt_map_switch( cbt_map_t* cbt_map );
int cbt_map_set( cbt_map_t* cbt_map, sector_t sector_start, sector_t sector_cnt );
int cbt_map_set_both( cbt_map_t* cbt_map, sector_t sector_start, sector_t sector_cnt );

size_t cbt_map_read_to_user( cbt_map_t* cbt_map, void __user * user_buffer, size_t offset, size_t size );

int map_print( page_array_t* map, size_t size );
void cbt_map_print( cbt_map_t* cbt_map);


static inline cbt_map_t* cbt_map_get_resource( cbt_map_t* cbt_map )
{
    if (cbt_map == NULL)
        return NULL;

    return (cbt_map_t*)shared_resource_get( &cbt_map->sharing_header );
}

static inline void cbt_map_put_resource( cbt_map_t* cbt_map )
{
    if (cbt_map != NULL)
        shared_resource_put( &cbt_map->sharing_header );
}

static inline void cbt_map_read_lock( cbt_map_t* cbt_map )
{
    down_read( &cbt_map->rw_lock );
};
static inline void cbt_map_read_unlock( cbt_map_t* cbt_map )
{
    up_read( &cbt_map->rw_lock );
};
static inline void cbt_map_write_lock( cbt_map_t* cbt_map )
{
    down_write( &cbt_map->rw_lock );
};
static inline void cbt_map_write_unlock( cbt_map_t* cbt_map )
{
    up_write( &cbt_map->rw_lock );
};
