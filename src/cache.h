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

extern i_cache icache;
extern d_cache dcache;

// 在i_cache中检查address是否命中,命中返回组中下标，未命中返回-1
int i_cache_check_hit(uint32_t address, i_cache *cache);

// 在d_cache中检查address是否命中,命中返回组中下标，未命中返回-1
int d_cache_check_hit(uint32_t address, d_cache *cache);

// 将拷贝的内存块写入(或替换)到i_cache中,返回写入的cacheline的组内下标
uint32_t i_cache_load_memory_impl(uint32_t address, i_cache *cache, uint8_t *mem_copied);

// 向i_cache中加载address所在的内存块，同时返回写入的cacheline的组内下标
uint32_t i_cache_load_memory(uint32_t address, i_cache *cache);

// 将拷贝的内存块写入(或替换)到d_cache中,返回写入的cacheline的组内下标
uint32_t d_cache_load_memory_impl(uint32_t address, d_cache *cache, uint8_t *mem_copied);

// 向d_cache中加载address所在的内存块,同时返回写入的cacheline的组内下标
uint32_t d_cache_load_memory(uint32_t address, d_cache *cache);

// 初始化i_cache
void init_i_cache(i_cache *cache);

// 初始化d_cache
void init_d_cache(d_cache *cache);

/*-----------------------------------------------------------------
从i_cache中读32位
这是一个集成函数，集成了是否命中、未命中时从内存加载、从cache中读取等步骤
最终效果就是读取内存中从address开始的32位
注意，由于shell.c中保证了对齐，此处不再做对齐操作
------------------------------------------------------------------*/
uint32_t i_cache_read32(uint32_t address, i_cache *cache);

/*-----------------------------------------------------------------
从d_cache中读32位
这是一个集成函数，集成了是否命中、未命中时从内存加载、从cache中读取等步骤
最终效果就是读取内存中从address开始的32位
注意，由于shell.c中保证了对齐，此处不再做对齐操作
------------------------------------------------------------------*/
uint32_t d_cache_read32(uint32_t address, d_cache *cache);

/*-----------------------------------------------------------------
向d_cache中写32位
这是一个集成函数，集成了是否命中、未命中时从内存加载、向cache中写入等步骤
最终效果就是从将32位写入address（实际写入cache，替换时回写）
注意，由于shell.c中保证了对齐，此处不再做对齐操作
------------------------------------------------------------------*/
void d_cache_write32(uint32_t address, uint32_t value, d_cache *cache);

#endif
