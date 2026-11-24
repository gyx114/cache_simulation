#include <stdbool.h>
#include <stdlib.h>

#include "cache.h"
#include "shell.h"

// 在i_cache中检查address是否命中,命中返回组中下标，未命中返回-1
int i_cache_check_hit(uint32_t address, i_cache *cache)
{
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    i_cache_line *cache_group = cache->data[group_idx];
    // 在cache组中遍历寻找
    int line_idx = -1;
    for (uint32_t i = 0; i < cache->ways; i++)
    {
        i_cache_line *line = cache_group + i;
        uint32_t target_tag = address >> (cache->group_bits + cache->offset_bits);
        if (line->valid && line->tag == target_tag)
        {
            line_idx = i;
            break;
        }
    }
    return line_idx;
}

// 在d_cache中检查address是否命中,命中返回组中下标，未命中返回-1
int d_cache_check_hit(uint32_t address, d_cache *cache)
{
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    d_cache_line *cache_group = cache->data[group_idx];
    // 在cache组中遍历寻找
    int line_idx = -1;
    for (uint32_t i = 0; i < cache->ways; i++)
    {
        d_cache_line *line = cache_group + i;
        uint32_t target_tag = address >> (cache->group_bits + cache->offset_bits);
        if (line->valid && line->tag == target_tag)
        {
            line_idx = i;
            break;
        }
    }
    return line_idx;
}

// 将拷贝的内存块写入(或替换)到i_cache中
void i_cache_load_memory_impl(uint32_t address, i_cache *cache, uint8_t *mem_copied)
{
    // 先检查有没有空闲
    uint32_t group_idx = address >> cache->offset_bits;
    group_idx &= ((1u << cache->group_bits) - 1);
    i_cache_line *cache_group = cache->data[group_idx];
    uint32_t target_tag = address >> (cache->group_bits + cache->offset_bits);
    // 在cache组中遍历，既找空闲，也顺便找lru最大的
    uint32_t idx_max_lru = 0; /*组中下标*/
    uint32_t max_lru = 0;
    bool found_empty = false;
    for (uint32_t i = 0; i < cache->ways; i++)
    {
        i_cache_line *line = cache_group + i;

        if (!line->valid) // 当前line空闲
        {
            if (!found_empty)
            {
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
        free(line->data); // 释放原有内存
        line->data = mem_copied;
        line->lru = 0; // 由于遍历时已经对所有way做了lru++,因此这里不用再修改其他way的lru
        line->tag = target_tag;
        line->valid = true;
    }
}

// 向i_cache中加载address所在的内存块
void i_cache_load_memory(uint32_t address, i_cache *cache)
{
    uint32_t mem_addr_start = address & ~((1u << cache->offset_bits) - 1);
    uint32_t block_size = 1u << cache->offset_bits;
    // 读取一块内存
    uint8_t *mem_copied = (uint8_t *)malloc(block_size);
    for (uint32_t i = 0; i < block_size; i += 4)
    {
        *((uint32_t *)(mem_copied + i)) = mem_read_32(mem_addr_start + i); // 一次拷贝4字节
    }

    i_cache_load_memory_impl(address, cache, mem_copied); // 装入cache
}

// 将拷贝的内存块写入(或替换)到d_cache中
void d_cache_load_memory_impl(uint32_t address, d_cache *cache, uint8_t *mem_copied)
{
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
        free(line->data); // 释放原有内存
        line->data = mem_copied;
        line->lru = 0; // 由于遍历时已经对所有way做了lru++,因此这里不用再修改其他way的lru
        line->tag = target_tag;
        line->valid = true;
        line->dirty = false;
    }
}

// 向d_cache中加载address所在的内存块
void d_cache_load_memory(uint32_t address, d_cache *cache)
{
    uint32_t mem_addr_start = address & ~((1u << cache->offset_bits) - 1);
    uint32_t block_size = 1u << cache->offset_bits;
    // 读取一块内存
    uint8_t *mem_copied = (uint8_t *)malloc(block_size);
    for (uint32_t i = 0; i < block_size; i += 4)
    {
        *((uint32_t *)(mem_copied + i)) = mem_read_32(mem_addr_start + i); // 一次拷贝4字节
    }

    d_cache_load_memory_impl(address, cache, mem_copied); // 装入cache
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
