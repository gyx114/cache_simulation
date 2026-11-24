#include <stdbool.h>
#include <stdlib.h>

#include "cache.h"
#include "shell.h"

i_cache icache;
d_cache dcache;

// 在i_cache中检查address是否命中,命中返回组中下标，未命中返回-1
int i_cache_check_hit(uint32_t address, i_cache *cache)
{
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    i_cache_line *cache_group = cache->data[group_idx];
    // 在cache组中遍历寻找
    uint32_t target_tag = address >> (cache->group_bits + cache->offset_bits);
    int idx_in_group = -1;
    for (uint32_t i = 0; i < cache->ways; i++)
    {
        i_cache_line *line = cache_group + i;
        if (line->valid && line->tag == target_tag)
        {
            idx_in_group = i;
            break;
        }
    }
    return idx_in_group;
}

// 在d_cache中检查address是否命中,命中返回组中下标，未命中返回-1
int d_cache_check_hit(uint32_t address, d_cache *cache)
{
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    d_cache_line *cache_group = cache->data[group_idx];
    // 在cache组中遍历寻找
    uint32_t target_tag = address >> (cache->group_bits + cache->offset_bits);
    int idx_in_group = -1;
    for (uint32_t i = 0; i < cache->ways; i++)
    {
        d_cache_line *line = cache_group + i;
        if (line->valid && line->tag == target_tag)
        {
            idx_in_group = i;
            break;
        }
    }
    return idx_in_group;
}

// 将拷贝的内存块写入(或替换)到i_cache中,返回写入的cacheline的组内下标
uint32_t i_cache_load_memory_impl(uint32_t address, i_cache *cache, uint8_t *mem_copied)
{
    uint32_t ret = 0;
    // 先检查有没有空闲
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    i_cache_line *cache_group = cache->data[group_idx];
    uint32_t target_tag = address >> (cache->group_bits + cache->offset_bits);
    // 在cache组中遍历，既找空闲，也顺便找lru最大的
    uint32_t idx_max_lru = 0; /*最大lru的组中下标*/
    uint32_t max_lru = 0;
    bool found_empty = false;
    for (uint32_t i = 0; i < cache->ways; i++)
    {
        i_cache_line *line = cache_group + i;

        if (!line->valid) // 当前line空闲
        {
            if (!found_empty) // 且之前没有空闲line
            {
                // 直接写入当前line
                ret = i;
                line->data = mem_copied;
                line->lru = 0;
                line->tag = target_tag;
                line->valid = true;
                found_empty = true;
            }
        }
        else
        {
            line->lru++;             // 顺便更新lru
            if (line->lru > max_lru) // 找max_lru
            {
                max_lru = line->lru;
                idx_max_lru = i;
            }
        }
    }

    if (!found_empty) // 没找到空闲，只能做lru替换
    {
        i_cache_line *line = cache_group + idx_max_lru;
        ret = idx_max_lru;
        free(line->data); // 释放原有内存
        line->data = mem_copied;
        line->lru = 0; // 由于遍历时已经对所有way做了lru++,因此这里不用再修改其他way的lru
        line->tag = target_tag;
        line->valid = true;
    }

    return ret;
}

// 向i_cache中加载address所在的内存块，同时返回写入的cacheline的组内下标
uint32_t i_cache_load_memory(uint32_t address, i_cache *cache)
{
    uint32_t mem_addr_start = address & ~((1u << cache->offset_bits) - 1);
    uint32_t block_size = 1u << cache->offset_bits;
    // 读取一块内存
    uint8_t *mem_copied = (uint8_t *)malloc(block_size);
    for (uint32_t i = 0; i < block_size; i += 4)
    {
        *((uint32_t *)(mem_copied + i)) = mem_read_32(mem_addr_start + i); // 一次拷贝4字节
    }

    return i_cache_load_memory_impl(address, cache, mem_copied); // 装入cache
}

// 将拷贝的内存块写入(或替换)到d_cache中,返回写入的cacheline的组内下标
uint32_t d_cache_load_memory_impl(uint32_t address, d_cache *cache, uint8_t *mem_copied)
{
    uint32_t ret = 0;
    // 先检查有没有空闲
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    d_cache_line *cache_group = cache->data[group_idx];
    uint32_t target_tag = address >> (cache->group_bits + cache->offset_bits);
    // 在cache组中遍历，既找空闲，也顺便找lru最大的
    uint32_t idx_max_lru = 0; /*组中下标*/
    uint32_t max_lru = 0;
    bool found_empty = false;
    for (uint32_t i = 0; i < cache->ways; i++)
    {
        d_cache_line *line = cache_group + i;

        if (!line->valid) // 当前line空闲
        {
            if (!found_empty)
            {
                ret = i;
                line->data = mem_copied;
                line->lru = 0;
                line->tag = target_tag;
                line->valid = true;
                line->dirty = false;
                found_empty = true;
            }
        }
        else
        {
            line->lru++;             // 顺便更新lru
            if (line->lru > max_lru) // 找max_lru
            {
                max_lru = line->lru;
                idx_max_lru = i;
            }
        }
    }
    if (!found_empty) // 没找到空闲，只能做lru替换
    {
        d_cache_line *line = cache_group + idx_max_lru;

        // d_cache有dirty位，需要判断是否需要写回内存
        if (line->dirty) // 需要写回
        {
            uint32_t mem_addr_start = (line->tag << (cache->group_bits + cache->offset_bits)) | (group_idx << cache->offset_bits);
            uint32_t block_size = 1u << cache->offset_bits;
            for (uint32_t i = 0; i < block_size; i += 4)
            {
                mem_write_32(mem_addr_start + i, *((uint32_t *)(line->data + i))); // 一次写4字节
            }
        }
        ret = idx_max_lru;
        free(line->data); // 释放原有内存
        line->data = mem_copied;
        line->lru = 0; // 由于遍历时已经对所有way做了lru++,因此这里不用再修改其他way的lru
        line->tag = target_tag;
        line->valid = true;
        line->dirty = false;
    }
    return ret;
}

// 向d_cache中加载address所在的内存块,同时返回写入的cacheline的组内下标
uint32_t d_cache_load_memory(uint32_t address, d_cache *cache)
{
    uint32_t mem_addr_start = address & ~((1u << cache->offset_bits) - 1);
    uint32_t block_size = 1u << cache->offset_bits;
    // 读取一块内存
    uint8_t *mem_copied = (uint8_t *)malloc(block_size);
    for (uint32_t i = 0; i < block_size; i += 4)
    {
        *((uint32_t *)(mem_copied + i)) = mem_read_32(mem_addr_start + i); // 一次拷贝4字节
    }

    return d_cache_load_memory_impl(address, cache, mem_copied); // 装入cache
}

// 初始化i_cache
void init_i_cache(i_cache *cache)
{
    cache->group_bits = I_CACHE_GROUP_BITS;
    cache->ways = I_CACHE_WAYS;
    cache->offset_bits = I_CACHE_OFFSET_BITS;
    cache->data = (i_cache_line **)malloc(sizeof(i_cache_line *) * (1u << I_CACHE_GROUP_BITS));
    for (uint32_t i = 0; i < (1u << I_CACHE_GROUP_BITS); i++)
    {
        cache->data[i] = (i_cache_line *)calloc(I_CACHE_WAYS, sizeof(i_cache_line)); // 组内分配每一行，置零
    }
}

// 初始化d_cache
void init_d_cache(d_cache *cache)
{
    cache->group_bits = D_CACHE_GROUP_BITS;
    cache->ways = D_CACHE_WAYS;
    cache->offset_bits = D_CACHE_OFFSET_BITS;
    cache->data = (d_cache_line **)malloc(sizeof(d_cache_line *) * (1u << D_CACHE_GROUP_BITS));
    for (uint32_t i = 0; i < (1u << D_CACHE_GROUP_BITS); i++)
    {
        cache->data[i] = (d_cache_line *)calloc(D_CACHE_WAYS, sizeof(d_cache_line)); // 组内分配每一行，置零
    }
}

/*-----------------------------------------------------------------
从i_cache中读32位
这是一个集成函数，集成了是否命中、未命中时从内存加载、从cache中读取等步骤
最终效果就是读取内存中从address开始的32位
注意，由于shell.c中保证了对齐，此处不再做对齐操作
------------------------------------------------------------------*/
uint32_t i_cache_read32(uint32_t address, i_cache *cache)
{
    int idx_in_group = i_cache_check_hit(address, cache); // 检查hit
    if (idx_in_group == -1)                               // 未命中
    {
        idx_in_group = i_cache_load_memory(address, cache); // 从内存装载
    }
    // 此时cache中已有想要的内容，可以取了
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    i_cache_line *cache_group = cache->data[group_idx];
    uint32_t offset_in_line = address & ((1u << cache->offset_bits) - 1);

    return *((uint32_t *)(cache_group[idx_in_group].data + offset_in_line));
}

/*-----------------------------------------------------------------
从d_cache中读32位
这是一个集成函数，集成了是否命中、未命中时从内存加载、从cache中读取等步骤
最终效果就是读取内存中从address开始的32位
注意，由于shell.c中保证了对齐，此处不再做对齐操作
------------------------------------------------------------------*/
uint32_t d_cache_read32(uint32_t address, d_cache *cache)
{
    int idx_in_group = d_cache_check_hit(address, cache); // 检查hit
    if (idx_in_group == -1)                               // 未命中
    {
        idx_in_group = d_cache_load_memory(address, cache); // 从内存装载
    }
    // 此时cache中已有想要的内容，可以取了
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    d_cache_line *cache_group = cache->data[group_idx];
    uint32_t offset_in_line = address & ((1u << cache->offset_bits) - 1);

    return *((uint32_t *)(cache_group[idx_in_group].data + offset_in_line));
}

/*-----------------------------------------------------------------
向d_cache中写32位
这是一个集成函数，集成了是否命中、未命中时从内存加载、向cache中写入等步骤
最终效果就是从将32位写入address（实际写入cache，替换时回写）
注意，由于shell.c中保证了对齐，此处不再做对齐操作
------------------------------------------------------------------*/
void d_cache_write32(uint32_t address, uint32_t value, d_cache *cache)
{
    int idx_in_group = d_cache_check_hit(address, cache); // 检查hit
    if (idx_in_group == -1)                               // 未命中
    {
        idx_in_group = d_cache_load_memory(address, cache); // 从内存装载
    }
    // 此时cache中已有对应块，可以写入了
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    d_cache_line *cache_group = cache->data[group_idx];
    uint32_t offset_in_line = address & ((1u << cache->offset_bits) - 1);
    *((uint32_t *)(cache_group[idx_in_group].data + offset_in_line)) = value;
    cache_group[idx_in_group].dirty = true; // dirty位标记
}
