#pragma once
#include "uuid_util.h"
#include "range.h"

int logging_init( const char* logdir );
void logging_done( void );
void logging_renew_check( void );
void log_s( const char* section, const unsigned level, const char* str );
void log_s_s( const char* section, const unsigned level, const char* str1, const char* str2 );
void log_s_d( const char* section, const unsigned level, const char* str, const int d );
void log_s_ld( const char* section, const unsigned level, const char* s, const long d );
void log_s_lld( const char* section, const unsigned level, const char* s, const long long d );
void log_s_sz( const char* section, const unsigned level, const char* s, const size_t d );
void log_s_x( const char* section, const unsigned level, const char* s, const int d );
void log_s_lx( const char* section, const unsigned level, const char* s, const long d );
void log_s_llx( const char* section, const unsigned level, const char* s, const long long d );
void log_s_p( const char* section, const unsigned level, const char* s, const void* p );
void log_s_dev_id( const char* section, const unsigned level, const char* s, const int major, const int minor );
//void log_s_uuid_bytes( const char* section, const unsigned level, const char* s, const __u8 b[16] );
void log_s_uuid(const char* section, const unsigned level, const char* s, const veeam_uuid_t* uuid);
void log_s_range( const char* section, const unsigned level, const char* s, const range_t* range );

void log_vformat( const char* section, const int level, const char *frm, va_list args );
void log_format( const char* section, const int level, const char* frm, ... );
///////////////////////////////////////////////////////////////////////////////
#define LOGGING_LEVEL_CMD 'CMD\0'
#define LOGGING_LEVEL_ERR 'ERR\0'
#define LOGGING_LEVEL_WRN 'WRN\0'
#define LOGGING_LEVEL_TR  'TR \0'

#define log_tr(msg) log_s(SECTION, LOGGING_LEVEL_TR, msg)
#define log_tr_s(msg, value) log_s_s(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_d(msg, value) log_s_d(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_ld(msg, value) log_s_ld(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_sz(msg, value) log_s_sz(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_lld(msg, value) log_s_ld(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_x(msg, value) log_s_x(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_p(msg, value) log_s_p(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_llx(msg, value) log_s_llx(SECTION, LOGGING_LEVEL_TR, msg, value)
#define log_tr_lx(msg, value) log_s_lx(SECTION, LOGGING_LEVEL_TR, msg, value)

#define log_tr_dev_id_s(msg, devid) log_s_dev_id(SECTION, LOGGING_LEVEL_TR, msg, devid.major, devid.minor)
#define log_tr_dev_t(msg, Device) log_s_dev_id(SECTION, LOGGING_LEVEL_TR, msg, MAJOR(Device), MINOR(Device) )

#define log_tr_sect(msg, value) log_s_llx(SECTION, LOGGING_LEVEL_TR, msg, (unsigned long long)value)
#define log_tr_uuid(msg, uuid) log_s_uuid(SECTION, LOGGING_LEVEL_TR, msg, uuid)
//#define log_tr_uuid_bytes(msg, b) log_s_uuid_bytes(SECTION, LOGGING_LEVEL_TR, msg, b)
#define log_tr_range(msg, range) log_s_range(SECTION, LOGGING_LEVEL_TR, msg, range)

///////////////////////////////////////////////////////////////////////////////
#define log_warn(msg) log_s(SECTION, LOGGING_LEVEL_WRN, msg)
#define log_warn_s(msg, value) log_s_s(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_d(msg, value) log_s_d(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_ld(msg, value) log_s_ld(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_sz(msg, value) log_s_sz(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_lld(msg, value) log_s_ld(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_x(msg, value) log_s_x(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_p(msg, value) log_s_p(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_llx(msg, value) log_s_llx(SECTION, LOGGING_LEVEL_WRN, msg, value)
#define log_warn_lx(msg, value) log_s_lx(SECTION, LOGGING_LEVEL_WRN, msg, value)

#define log_warn_dev_id_s(msg, devid) log_s_dev_id(SECTION, LOGGING_LEVEL_WRN, msg, devid.major, devid.minor)
#define log_warn_dev_t(msg, Device) log_s_dev_id(SECTION, LOGGING_LEVEL_WRN, msg, MAJOR(Device), MINOR(Device) )

#define log_warn_sect(msg, value) log_s_llx(SECTION, LOGGING_LEVEL_WRN, msg, (unsigned long long)value)
#define log_warn_uuid(msg, uuid) log_s_uuid(SECTION, LOGGING_LEVEL_WRN, msg, uuid)
//#define log_warn_uuid_bytes(msg, b) log_s_uuid_bytes(SECTION, LOGGING_LEVEL_WRN, msg, b)
#define log_warn_range(msg, range) log_s_range(SECTION, LOGGING_LEVEL_WRN, msg, range)
//////////////////////////////////////////////////////////////////////////
#define log_err(msg) log_s(SECTION, LOGGING_LEVEL_ERR, msg)
#define log_err_s(msg, value) log_s_s(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_d(msg, value) log_s_d(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_ld(msg, value) log_s_ld(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_lld(msg, value) log_s_lld(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_sz(msg, value) log_s_sz(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_x(msg, value) log_s_x(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_p(msg, value) log_s_p(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_llx(msg, value) log_s_llx(SECTION, LOGGING_LEVEL_ERR, msg, value)
#define log_err_lx(msg, value) log_s_lx(SECTION, LOGGING_LEVEL_ERR, msg, value)

#define log_err_dev_id_s(msg, devid) log_s_dev_id(SECTION, LOGGING_LEVEL_ERR, msg, devid.major, devid.minor)
#define log_err_dev_t(msg, Device) log_s_dev_id(SECTION, LOGGING_LEVEL_ERR, msg, MAJOR(Device), MINOR(Device) )

#define log_err_sect(msg, value) log_s_lld(SECTION, LOGGING_LEVEL_ERR, msg, (unsigned long long)value)
#define log_err_uuid(msg, uuid) log_s_uuid(SECTION, LOGGING_LEVEL_ERR, msg, uuid)
//#define log_err_uuid_bytes(msg, b) log_s_uuid_bytes(SECTION, LOGGING_LEVEL_ERR, msg, b)
#define log_err_range(msg, range) log_s_range(SECTION, LOGGING_LEVEL_ERR, msg, range)
//////////////////////////////////////////////////////////////////////////
//void log_dump(void* p, size_t size);
