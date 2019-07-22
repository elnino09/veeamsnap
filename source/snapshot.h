#pragma once
#include "container.h"


typedef struct snapshot_s
{
    content_t content;
    unsigned long long id;

    dev_t* dev_id_set;    // 需要创建快照的块设备dev_t列表
    int dev_id_set_size;  // length of dev_id_set
}snapshot_t;

int snapshot_Init( void );
int snapshot_Done( void );

int snapshot_FindById( unsigned long long id, snapshot_t** psnapshot );

int snapshot_Create( dev_t* dev_id_set, unsigned int dev_id_set_size, unsigned int cbt_block_size_degree, unsigned long long* psnapshot_id );

int snapshot_Destroy( unsigned long long snapshot_id );


