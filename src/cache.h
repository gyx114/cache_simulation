#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stdbool.h>

/*i_cache参数*/
#define I_CACHE_GROUP_BITS 6
#define I_CACHE_WAYS 4
#define I_CACHE_OFFSET_BITS 5

/*d_cache参数*/
#define D_CACHE_GROUP_BITS 8
#define D_CACHE_WAYS 8
#define D_CACHE_OFFSET_BITS 5

typedef struct i_cache_line
{
    bool valid;
    uint32_t tag; // PC[31:11]
    uint32_t lru; // lru计数器
    uint8_t *data;
} i_cache_line;

typedef struct d_cache_line
{
    bool valid;
    bool dirty;
    uint32_t tag; // address[31:13]
    uint32_t lru; // lru计数器
    uint8_t *data;
} d_cache_line;

typedef struct i_cache
{
    uint32_t group_bits;  // 地址中有几位用于分组
    uint32_t ways;        // 每组有几个cache_line
    uint32_t offset_bits; // 地址中有几位用于块内offset
    i_cache_line **data;

} i_cache;

typedef struct d_cache
{
    uint32_t group_bits;  // 地址中有几位用于分组
    uint32_t ways;        // 每组有几个cache_line
    uint32_t offset_bits; // 地址中有几位用于块内offset
    d_cache_line **data;

} d_cache;

#endif
