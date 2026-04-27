#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace memoryPool
{

void* ThreadCache::allocate(size_t size) {
    if (size == 0) {
        size = ALIGNMENT; // 最小分配单位为ALIGNMENT
    }
    if (size > MAX_BYTES) {
        return malloc(size); // 超过最大限制，直接使用全局分配器
    }

    size_t index = SizeClass::getIndex(size); // 计算自由链表索引
    m_freeListSize[index]--; // 减少该索引的可用块数量
    void* ptr = m_freeList[index]; // 从自由链表中获取一个块
    if (ptr != nullptr) {
        m_freeList[index] = *reinterpret_cast<void**>(ptr); // 更新自由链表头
        return ptr; // 返回分配的块
    }
    // 自由链表为空，从中心缓存获取新块
    return fetchFromCentralCache(index); 
}

void ThreadCache::deallocate(void* ptr, size_t size) {
    if (ptr == nullptr) {
        return; // 空指针不需要处理
    }
    if (size > MAX_BYTES) {
        free(ptr); // 超过最大限制，直接使用全局释放器
        return;
    }

    size_t index = SizeClass::getIndex(size); // 计算自由链表索引
    *reinterpret_cast<void**>(ptr) = m_freeList[index]; // 将当前块加入自由链表
    m_freeList[index] = ptr; // 更新自由链表头
    m_freeListSize[index]++; // 增加该索引的可用块数量

    if (shouldReturnToCentralCache(index)) {
        returnToCentralCache(m_freeList[index], size); // 将块返回给中心缓存
    }
}

void* ThreadCache::fetchFromCentralCache(size_t index) {
    // 从中心缓存批量获取内存
    void* start = CentralCache::getInstance().fetchRange(index);
    if (!start) return nullptr;

    // 取一个返回，其余放入自由链表
    void* result = start;
    m_freeList[index] = *reinterpret_cast<void**>(start);
    
    // 更新自由链表大小
    size_t batchNum = 0;
    void* current = start; // 从start开始遍历

    // 计算从中心缓存获取的内存块数量
    while (current != nullptr)
    {
        batchNum++;
        current = *reinterpret_cast<void**>(current); // 遍历下一个内存块
    }

    // 更新freeListSize_，增加获取的内存块数量
    m_freeListSize[index] += batchNum;
    
    return result;
}

void ThreadCache::returnToCentralCache(void* start, size_t size) {
    // 根据大小计算对应的索引
    size_t index = SizeClass::getIndex(size);

    // 获取对齐后的实际块大小
    size_t alignedSize = SizeClass::roundUp(size);

    // 计算要归还内存块数量
    size_t batchNum = m_freeListSize[index];
    if (batchNum <= 1) return; // 如果只有一个块，则不归还

    // 保留一部分在ThreadCache中（比如保留1/4）
    size_t keepNum = std::max(batchNum / 4, size_t(1));
    size_t returnNum = batchNum - keepNum;

    // 将内存块串成链表
    char* current = static_cast<char*>(start);
    // 使用对齐后的大小计算分割点
    char* splitNode = current;
    for (size_t i = 0; i < keepNum - 1; ++i) 
    {
        splitNode = reinterpret_cast<char*>(*reinterpret_cast<void**>(splitNode));
        if (splitNode == nullptr) 
        {
            // 如果链表提前结束，更新实际的返回数量
            returnNum = batchNum - (i + 1);
            break;
        }
    }

    if (splitNode != nullptr) 
    {
        // 将要返回的部分和要保留的部分断开
        void* nextNode = *reinterpret_cast<void**>(splitNode);
        *reinterpret_cast<void**>(splitNode) = nullptr; // 断开连接

        // 更新ThreadCache的空闲链表
        m_freeList[index] = start;

        // 更新自由链表大小
        m_freeListSize[index] = keepNum;

        // 将剩余部分返回给CentralCache
        if (returnNum > 0 && nextNode != nullptr)
        {
            CentralCache::getInstance().returnRange(nextNode, returnNum * alignedSize, index);
        }
    }
}

bool ThreadCache::shouldReturnToCentralCache(size_t index) {
    // 设定阈值，例如：当自由链表的大小超过一定数量时
    size_t threshold = 256; 
    return (m_freeListSize[index] > threshold);
}



} // namespace memoryPool