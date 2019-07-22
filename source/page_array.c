#include "stdafx.h"
#include "page_array.h"

#define SECTION "page_array"
//#include "log_format.h"

atomic64_t page_alloc_count;        // 分配了多少个page_info_t
atomic64_t page_free_count;         //
atomic64_t page_array_alloc_count;  // 分配了多少个page_array_t
atomic64_t page_array_free_count;

void page_arrays_init( void )
{
    atomic64_set( &page_alloc_count, 0 );
    atomic64_set( &page_free_count, 0 );

    atomic64_set( &page_array_alloc_count, 0 );
    atomic64_set( &page_array_free_count, 0 );
}

void page_arrays_print_state( void )
{
    log_tr( "" );
    log_tr( "Page array state:" );
    log_tr_lld( "pages allocated: ", (long long int)atomic64_read( &page_alloc_count ) );
    log_tr_lld( "pages freed: ", (long long int)atomic64_read( &page_free_count ) );
    log_tr_lld( "pages in use: ", (long long int)atomic64_read( &page_alloc_count ) - (long long int)atomic64_read( &page_free_count ) );

    log_tr_lld( "arrays allocated: ", (long long int)atomic64_read( &page_array_alloc_count ) );
    log_tr_lld( "arrays freed: ", (long long int)atomic64_read( &page_array_free_count ) );
    log_tr_lld( "arrays in use: ", (long long int)atomic64_read( &page_array_alloc_count ) - (long long int)atomic64_read( &page_array_free_count ) );
}

page_array_t* page_array_alloc( size_t count, int gfp_opt )
{
    int res = SUCCESS;
    size_t inx;
    page_array_t* arr = NULL;
    while (NULL == (arr = dbg_kzalloc( sizeof( page_array_t ) + count*sizeof( page_info_t ), gfp_opt ))){  // 因为 page_array_t结构体中第二个成员定义为page_info_t pg[0]
                                                                                                             // 所以sizeof( page_array_t )不会计算pg，需要+count*sizeof( page_info_t )
        log_err( "Failed to allocate page_array buffer" );
        return NULL;
    }
    arr->pg_cnt = count;
    for (inx = 0; inx < arr->pg_cnt; ++inx){
        if (NULL == (arr->pg[inx].page = alloc_page( gfp_opt ))){
            log_err( "Failed to allocate page" );
            res = -ENOMEM;
            break;
        }
        arr->pg[inx].addr = page_address( arr->pg[inx].page );
        atomic64_inc( &page_alloc_count );
    }
    atomic64_inc( &page_array_alloc_count );

    if (SUCCESS == res)
        return arr;
    
    page_array_free( arr );
    return NULL;
}

void page_array_free( page_array_t* arr )
{
    size_t inx;
    size_t count = arr->pg_cnt;
    if (arr == NULL)
        return;

    for (inx = 0; inx < count; ++inx){
        if (arr->pg[inx].page != NULL){
            free_page( (unsigned long)(arr->pg[inx].addr) );
            atomic64_inc( &page_free_count );
        }
    }
    dbg_kfree( arr );
    atomic64_inc( &page_array_free_count );
}

/*
 * 将页组里从arr_ofs（单位byte）开始长度为length（单位byte）的数据拷到dst地址上
 */
size_t page_array_pages2mem( void* dst, size_t arr_ofs, page_array_t* arr, size_t length )
{
    int page_inx = arr_ofs / PAGE_SIZE;
    size_t processed_len = 0;
    void* src;
    {//first
        size_t unordered = arr_ofs & (PAGE_SIZE - 1);  // page_idx是位移值arr_ofs对应的页索引，unordered是页索引里的偏移
        size_t page_len = min_t( size_t, ( PAGE_SIZE - unordered ), length );  // 如果length比较大会跨页，那么先把当前页内的数据copy，
                                                                               // 接下来就可以按页大小拷贝了
        src = arr->pg[page_inx].addr;
        memcpy( dst + processed_len, src + unordered, page_len );

        ++page_inx;
        processed_len += page_len;
    }
    while ((processed_len < length) && (page_inx < arr->pg_cnt))
    {
        size_t page_len = min_t( size_t, PAGE_SIZE, (length - processed_len) );

        src = arr->pg[page_inx].addr;
        memcpy( dst + processed_len, src, page_len );

        ++page_inx;
        processed_len += page_len;
    }

    return processed_len;
}

/*
 * 将从src开始长度为length（单位byte）的数据拷到页组从arr_ofs开始的地址上
 */
size_t page_array_mem2pages( void* src, size_t arr_ofs, page_array_t* arr, size_t length )
{
    int page_inx = arr_ofs / PAGE_SIZE;
    size_t processed_len = 0;
    void* dst;
    {//first
        size_t unordered = arr_ofs & (PAGE_SIZE - 1);
        size_t page_len = min_t( size_t, (PAGE_SIZE - unordered), length );

        dst = arr->pg[page_inx].addr;
        memcpy( dst + unordered, src + processed_len, page_len );

        ++page_inx;
        processed_len += page_len;
    }
    while ((processed_len < length) && (page_inx < arr->pg_cnt))
    {
        size_t page_len = min_t( size_t, PAGE_SIZE, (length - processed_len) );

        dst = arr->pg[page_inx].addr;
        memcpy( dst, src + processed_len, page_len );

        ++page_inx;
        processed_len += page_len;
    }

    return processed_len;
}

/*
 * 将从src开始长度为length（单位byte）的数据拷到页组从dst_user开始的用户空间地址上
 */
size_t page_array_page2user( char __user* dst_user, size_t arr_ofs, page_array_t* arr, size_t length )
{
    size_t left_data_length;
    int page_inx = arr_ofs / PAGE_SIZE;
    size_t processed_len = 0;

    size_t unordered = arr_ofs & (PAGE_SIZE - 1);
    if (unordered != 0)//first
    {
        size_t page_len = min_t( size_t, (PAGE_SIZE - unordered), length );

        left_data_length = copy_to_user( dst_user + processed_len, arr->pg[page_inx].addr  + unordered, page_len );
        if (0 != left_data_length){
            log_err( "Failed to copy data from page array to user buffer" );
            return processed_len;
        }

        ++page_inx;
        processed_len += page_len;
    }
    while ((processed_len < length) && (page_inx < arr->pg_cnt))
    {
        size_t page_len = min_t( size_t, PAGE_SIZE, (length - processed_len) );

        left_data_length = copy_to_user( dst_user + processed_len, arr->pg[page_inx].addr, page_len );
        if (0 != left_data_length){
            log_err( "Failed to copy data from page array to user buffer" );
            break;
        }

        ++page_inx;
        processed_len += page_len;
    }

    return processed_len;
}

/*
 * 将从用户空间地址src开始长度为length（单位byte）的数据拷到页组从arr_ofs开始的地址上
 */
size_t page_array_user2page( const char __user* src_user, size_t arr_ofs, page_array_t* arr, size_t length )
{
    size_t left_data_length;
    int page_inx = arr_ofs / PAGE_SIZE;
    size_t processed_len = 0;

    size_t unordered = arr_ofs & (PAGE_SIZE - 1);
    if (unordered != 0)//first
    {
        size_t page_len = min_t( size_t, (PAGE_SIZE - unordered), length );

        left_data_length = copy_from_user( arr->pg[page_inx].addr + unordered, src_user + processed_len, page_len );
        if (0 != left_data_length){
            log_err( "Failed to copy data from page array to user buffer" );
            return processed_len;
        }

        ++page_inx;
        processed_len += page_len;
    }
    while ((processed_len < length) && (page_inx < arr->pg_cnt))
    {
        size_t page_len = min_t( size_t, PAGE_SIZE, (length - processed_len) );

        left_data_length = copy_from_user( arr->pg[page_inx].addr, src_user + processed_len, page_len );
        if (0 != left_data_length){
            log_err( "Failed to copy data from page array to user buffer" );
            break;
        }

        ++page_inx;
        processed_len += page_len;
    }

    return processed_len;
}

/*
 * 计算buffer_size个字节需要占用几个页
 */
size_t page_count_calc( size_t buffer_size )
{
    size_t page_count = buffer_size / PAGE_SIZE;

    if ( buffer_size & (PAGE_SIZE - 1) )
        page_count += 1;
    return page_count;
}

/*
 * 计算range_cnt_sect个扇区需要占用几个页
 */
size_t page_count_calc_sectors( sector_t range_start_sect, sector_t range_cnt_sect )
{
    size_t page_count = range_cnt_sect / SECTORS_IN_PAGE;

    if (unlikely( range_cnt_sect & (SECTORS_IN_PAGE - 1) ))
        page_count += 1;
    return page_count;
}

/*
 * 根据element在arr中的索引（以sizeof element为单位）求得element的地址
 * index: 该element是第几个element（从第0页开始算）（以sizeof element为单位）
 * sizeof_element: 该element的大小
 */
void* page_get_element( page_array_t* arr, size_t index, size_t sizeof_element )
{
    size_t elements_in_page = PAGE_SIZE / sizeof_element;  // 每页有几个element
    size_t pg_inx = index / elements_in_page;              // 该element在第几页
    size_t pg_ofs = (index - (pg_inx * elements_in_page)) * sizeof_element;  // 该element在页内的位移（单位：byte）

    return (arr->pg[pg_inx].addr + pg_ofs);
}

/*
 * 根据扇区在页组中的索引（以sizeof 扇区为单位）求得其地址
 * arr_ofs: 第几个element（从第0页开始算）（以扇区为单位）
 */
char* page_get_sector( page_array_t* arr, sector_t arr_ofs )
{
    size_t pg_inx = arr_ofs >> (PAGE_SHIFT - SECTOR512_SHIFT);  // 该扇区在第几个页
    size_t pg_ofs = sector_to_size( arr_ofs & ((1 << (PAGE_SHIFT - SECTOR512_SHIFT)) - 1) );  // 该扇区在页内的位移（单位：byte）

    return (arr->pg[pg_inx].addr + pg_ofs);
}

/*
 * 将页组全部置为value
 */
void page_array_memset( page_array_t* arr, int value )
{
    size_t inx;
    for (inx = 0; inx < arr->pg_cnt; ++inx){
        void* ptr = arr->pg[inx].addr;
        memset( ptr, value, PAGE_SIZE );
    }
}

void page_array_memcpy( page_array_t* dst, page_array_t* src )
{
    size_t inx;
    size_t count = min_t( size_t, dst->pg_cnt, src->pg_cnt );

    for (inx = 0; inx < count; ++inx){
        void* dst_ptr = dst->pg[inx].addr ;
        void* src_ptr = src->pg[inx].addr;
        memcpy( dst_ptr, src_ptr, PAGE_SIZE );
    }
}

/*
 * 检查页索引page_inx是否在指定页组内（是否超过页组范围）
 */
#define _PAGE_INX_CHECK(arr, inx, page_inx) \
if (page_inx >= arr->pg_cnt){ \
    log_err_sz( "Invalid index ", inx ); \
    log_err_sz( "page_inx=", page_inx ); \
    log_err_sz( "page_cnt=", arr->pg_cnt ); \
    return -ENODATA; \
}

/*
 * 每页能存放几个指针
 */
#define POINTERS_IN_PAGE (PAGE_SIZE/sizeof(void*))

/*
 * 根据指针在页组中的索引（单位：sizeof指针）求得其地址
 * inx: 第几个指针（从第0页开始算）（以sizeof指针为单位）
 */
int page_array_ptr_get(page_array_t* arr, size_t inx, void** value)
{

    size_t page_inx = inx / POINTERS_IN_PAGE;  // 要找的指针在第几个页？
    _PAGE_INX_CHECK(arr, inx, page_inx);

    {
        size_t pos = inx & (POINTERS_IN_PAGE - 1);  // 要找的指针在页内位移（单位：sizeof指针）
        void** ptr = arr->pg[page_inx].addr;
        *value = ptr[pos];
    }
    return SUCCESS;
}

/*
 * 将页组第inx个指针赋值为value
 */
int page_array_ptr_set(page_array_t* arr, size_t inx, void* value)
{
    size_t page_inx = inx / POINTERS_IN_PAGE;
    _PAGE_INX_CHECK(arr, inx, page_inx);

    {
        size_t byte_pos = inx & (POINTERS_IN_PAGE - 1);
        void** ptr = arr->pg[page_inx].addr;
        ptr[byte_pos] = value;
    }
    return SUCCESS;
}

/*
 * 根据byte在页组中的索引（单位：sizeof byte）求得其地址
 * inx: 第几个byte（从第0页开始算）（以sizeof byte为单位）
 */
int page_array_byte_get( page_array_t* arr, size_t inx, byte_t* value )
{
    size_t page_inx = inx >> PAGE_SHIFT;
    _PAGE_INX_CHECK( arr, inx, page_inx );

    {
        size_t byte_pos = inx & (PAGE_SIZE - 1);
        byte_t* ptr = arr->pg[page_inx].addr;
        *value = ptr[byte_pos];
    }
    return SUCCESS;
}

/*
 * 将页组第inx个byte赋值为value
 */
int page_array_byte_set( page_array_t* arr, size_t inx, byte_t value )
{
    size_t page_inx = inx >> PAGE_SHIFT;
    _PAGE_INX_CHECK( arr, inx, page_inx );

    {
        size_t byte_pos = inx & (PAGE_SIZE - 1);
        byte_t* ptr = arr->pg[page_inx].addr;
        ptr[byte_pos] = value;
    }
    return SUCCESS;
}

/*
 * 根据bit在页组中的索引（单位：sizeof bit）求得其地址
 * inx: 第几个bit（从第0页开始算）（以sizeof bit为单位）
 */
int page_array_bit_get( page_array_t* arr, size_t inx, bool* value )
{
    byte_t v;
    size_t byte_inx = (inx / BITS_PER_BYTE);
    int res = page_array_byte_get( arr, byte_inx, &v );
    if (SUCCESS != res)
        return res;

    {
        size_t bit_inx = (inx & (BITS_PER_BYTE - 1));
        *value = v & (1 << bit_inx);
    }
    return SUCCESS;
}

/*
 * 将页组第inx个bit赋值为value
 */
int page_array_bit_set( page_array_t* arr, size_t inx, bool value )
{
    size_t byte_inx = (inx / BITS_PER_BYTE);
    size_t page_inx = byte_inx >> PAGE_SHIFT;
    _PAGE_INX_CHECK( arr, inx, page_inx );

    {
        byte_t v;
        size_t bit_inx = (inx & (BITS_PER_BYTE - 1));

        size_t byte_pos = byte_inx & (PAGE_SIZE - 1);
        byte_t* ptr = arr->pg[page_inx].addr;

        v = ptr[byte_pos];
        if (value){
            v |= (1 << bit_inx);
        }
        else{
            v &= ~(1 << bit_inx);
        }

        ptr[byte_pos] = v;
    }
    return SUCCESS;
}
