#pragma once

#include <atomic>
#include <chrono>
#include "Common.h"

namespace memoryPool 
{

struct SpanTracker {
    std::atomic<void*> spanAddress{nullptr};  // 追踪的内存块地址
    std::atomic<size_t> numPages{0};          // 追踪的内存块大小
    std::atomic<size_t> blockCount{0};        // 追踪的内存块数量
    std::atomic<size_t> freeCount{0};         // 追踪的空闲块数量
};


// 线程本地缓存
class CentralCache
{
public:
    static CentralCache& getInstance() {
        static CentralCache instance; // 中心缓存是全局共享的单例实例
        return instance;
    }

    void* fetchRange(size_t index);  // 从中心缓存获取内存块范围
    void returnRange(void* start, size_t size, size_t index); // 将内存块范围返回给中心缓存

private:
    CentralCache();  // 私有构造函数，禁止外部实例化

    // 从页面缓存获取内存块
    void* fetchFromPageCache(size_t index);

    // 获取内存块对应的追踪器
    SpanTracker* getSpanTracker(void* blockAddr);

    // 更新追踪器的空闲块数量并检查是否可以归还给页面缓存
    void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);

private:
    // 中心缓存的自由链表，使用原子指针以支持多线程访问
    std::array<std::atomic<void*>, FREE_LIST_SIZE> m_centralFreeList;

    // 中心缓存的锁，使用原子标志以支持多线程访问
    std::array<std::atomic_flag, FREE_LIST_SIZE> m_centralFreeListLocks;
    
    // 追踪分配的内存块，方便合并和回收
    std::array<SpanTracker, 1024> m_spanTrackers;
    std::atomic<size_t> m_spanCount{0};

    // 延迟归还相关的成员变量
    static const size_t MAX_DELAY_COUNT = 48;  // 最大延迟计数
    std::array<std::atomic<size_t>, FREE_LIST_SIZE> m_delayCounts;  // 每个大小类的延迟计数
    std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> m_lastReturnTimes;  // 上次归还时间
    static const std::chrono::milliseconds DELAY_INTERVAL;  // 延迟间隔
    
    // 判断是否需要执行延迟归还
    bool shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);
    void performDelayedReturn(size_t index);
};
} // namespace memoryPool