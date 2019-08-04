#include "stdafx.h"
#include "page_array.h"
#include "blk_descr_array.h"

#define SECTION "blk_descr "
#include "log_format.h"

/*
 * first: 起始bit
 * last: 结束bit
 */
int blk_descr_array_init( blk_descr_array_t* header, blk_descr_array_index_t first, blk_descr_array_index_t last )
{
    size_t page_count = 0;
    init_rwsem( &header->rw_lock );

    header->first = first;
    header->last = last;

    // 每个块用一个bit表示，容纳这么多bit需要多少个group？（每个group可容纳256bit）
    header->group_count = (size_t)((last + 1 - first) >> BLK_DESCR_GROUP_LENGTH_SHIFT);  // / 256

    if ((last + 1 - first) & BLK_DESCR_GROUP_LENGTH_MASK)
        ++(header->group_count);

    // 容纳这么多group的指针需要多少个页？
    page_count = page_count_calc(header->group_count * sizeof(blk_descr_array_group_t*));
    header->groups = page_array_alloc( page_count, GFP_KERNEL);
    if (NULL == header->groups){
        blk_descr_array_done( header );
        return -ENOMEM;
    }
    page_array_memset(header->groups, 0);

    log_tr_format("Block description array takes up %lu pages", page_count);

    return SUCCESS;
}

void blk_descr_array_done( blk_descr_array_t* header )
{
    if (header->groups != NULL){

        blk_descr_array_reset( header );

        page_array_free(header->groups);

        header->groups = NULL;
    }
}

/*
 * 将header->groups里的blk_descr_array_group_t指针全部释放
 */
void blk_descr_array_reset( blk_descr_array_t* header )
{
    size_t gr_idx;
    if (header->groups != NULL){
        for (gr_idx = 0; gr_idx < header->group_count; ++gr_idx){
            void* group = NULL;
            if (SUCCESS == page_array_ptr_get(header->groups, gr_idx, &group)){
                if (group != NULL){
                    dbg_kfree(group);
                    page_array_ptr_set(header->groups, gr_idx, NULL);
                }
            }
        }
    }
}

/*
 * 将第inx个数据设置为value
 */
int blk_descr_array_set( blk_descr_array_t* header, blk_descr_array_index_t inx, blk_descr_array_el_t value )
{
    int res = SUCCESS;

    down_write( &header->rw_lock );
    do{
        size_t gr_idx;
        size_t val_idx;
        blk_descr_array_group_t* group = NULL;
        unsigned char bits;

        if (!((header->first <= inx) && (inx <= header->last))){
            res = -EINVAL;
            break;
        }

        gr_idx = (size_t)((inx - header->first) >> BLK_DESCR_GROUP_LENGTH_SHIFT);     // 先求出在第几个group
        if (SUCCESS != page_array_ptr_get(header->groups, gr_idx, (void**)&group)){   // 再从page_array中取出group指针
            res = -EINVAL;
            break;
        }
        if (group == NULL){
            // group还没分配内存的话则分配内存
            group = dbg_kzalloc(sizeof(blk_descr_array_group_t), GFP_NOIO);
            if (group == NULL){
                res = -ENOMEM;
                break;
            }
            if (SUCCESS != page_array_ptr_set(header->groups, gr_idx, group)){
                res = -EINVAL;
                break;
            }
        }
        val_idx = (size_t)((inx - header->first) & BLK_DESCR_GROUP_LENGTH_MASK);  // 这个是在group内偏移

        bits = (1 << (val_idx & 0x7));  // 这个是在字节内的偏移
        if (group->bitmap[val_idx >> 3] & bits){  // val_idx >> 3 是第几个字节
            // rewrite
        }
        else{
            group->bitmap[val_idx >> 3] |= bits;
            ++group->cnt;
        }
        group->values[val_idx] = value;


    } while (false);
    up_write( &header->rw_lock );

    return res;
}

/*
 * 获取第inx个数据
 */
int blk_descr_array_get( blk_descr_array_t* header, blk_descr_array_index_t inx, blk_descr_array_el_t* p_value )
{
    int res = SUCCESS;

    down_read(&header->rw_lock);
    do{
        size_t gr_idx;
        size_t val_idx;
        blk_descr_array_group_t* group = NULL;
        unsigned char bits;

        if ((inx < header->first) || (header->last < inx)){
            res = -EINVAL;
            break;
        }

        // 每256个块组成一个group，先求出在第几个group
        gr_idx = (size_t)((inx - header->first) >> BLK_DESCR_GROUP_LENGTH_SHIFT);

        if (SUCCESS != page_array_ptr_get(header->groups, gr_idx, (void**)&group)){
            res = -EINVAL;
            break;
        }
        if (group == NULL){
            res = -ENODATA;
            break;
        }

        val_idx = (size_t)((inx - header->first) & BLK_DESCR_GROUP_LENGTH_MASK);  // 其实就是 (inx - header->first) / 256 的余。。
                                                                                  // 这就是块在group内的第几bit
        bits = (1 << (val_idx & 0x7));  // (val_idx & 0x7) 就是块在byte内的第几位
        if (group->bitmap[val_idx >> 3] & bits)  // val_idx >> 3 就是块在group内的第几个byte
            *p_value = group->values[val_idx];
        else{
            res = -ENODATA;
            break;
        }
    } while (false);
    up_read(&header->rw_lock);

    return res;
}

