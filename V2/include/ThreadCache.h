#pragma once

#include "Common.h"

namespace memoryPool 
{

// 线程本地缓存
class ThreadCache
{
public:
    static ThreadCache* getInstance() {
        static thread_local ThreadCache instance; // 每个线程都有一个独立的实例
        return &instance;
    }

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);

private:
    ThreadCache() {
        m_freeList.fill(nullptr); // 初始化自由链表
        m_freeListSize.fill(0);  // 初始化大小统计
    }

    // 从中心缓存获取内存块
    void* fetchFromCentralCache(size_t index);
    // 将内存块返回给中心缓存
    void returnToCentralCache(void* start, size_t size);
    // 判断是否需要将内存块返回给中心缓存
    bool shouldReturnToCentralCache(size_t index);

private:
    std::array<void*, FREE_LIST_SIZE> m_freeList;  // 自由链表
    std::array<size_t, FREE_LIST_SIZE> m_freeListSize;  // 自由链表大小统计
};

} // namespace memoryPool