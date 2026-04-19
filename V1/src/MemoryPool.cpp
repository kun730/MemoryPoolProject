#include "../include/MemoryPool.h"

namespace memoryPool
{
MemoryPool::MemoryPool(size_t blockSize) 
    : m_blockSize(blockSize)
    , m_slotSize(0)
    , m_freeList(nullptr)
    , m_curSlot(nullptr)
    , m_firstBlock(nullptr)
    , m_lastSlot(nullptr) 
{}

MemoryPool::~MemoryPool() {
    Slot* cur = m_firstBlock;
    while (cur) {
        Slot* next = cur->next;
        operator delete(cur);
        cur = next;
    }
}

void MemoryPool::init(size_t size) {
    assert(size > 0);
    m_slotSize = size;
    m_firstBlock = nullptr;
    m_curSlot = nullptr;
    m_freeList.store(nullptr, std::memory_order_relaxed);
    m_lastSlot = nullptr;
}

void* MemoryPool::allocate() {
    // 先尝试从freeList中分配
    Slot* freeSlot = popFreeList();
    if (freeSlot) {
        return freeSlot;
    }

    // 如果freeList中没有可用的槽，则尝试从当前内存块中分配
    std::lock_guard<std::mutex> lock(m_mutexForCurSlot);
    if (m_curSlot >= m_lastSlot) {
        allocateNewBlock();
    }
    void* result = m_curSlot;
    m_curSlot = reinterpret_cast<Slot*>(reinterpret_cast<char*>(m_curSlot) + m_slotSize);
    return result;
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) {
        return ;
    }
    Slot* slot = static_cast<Slot*>(ptr);
    pushFreeList(slot);
}

void MemoryPool::allocateNewBlock() {
    // 头插法插入新的内存块
    Slot* newBlock = static_cast<Slot*>(operator new(m_blockSize));
    newBlock->next = m_firstBlock;
    m_firstBlock = newBlock;

    // 更新curSlot和lastSlot
    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    size_t paddingSize = padPointer(body, m_slotSize);  // 计算对齐需要填充内存的大小
    m_curSlot = reinterpret_cast<Slot*>(body + paddingSize);

    m_lastSlot = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + m_blockSize - m_slotSize + 1);
    m_freeList = nullptr;
}

size_t MemoryPool::padPointer(char* ptr, size_t alignment) {
    return (alignment - reinterpret_cast<size_t>(ptr)) % alignment;
}

// 实现无锁入队操作
bool MemoryPool::pushFreeList(Slot* slot) {
    while(true) {
        Slot* oldHead = m_freeList.load(std::memory_order_relaxed);
        slot->next.store(oldHead, std::memory_order_relaxed);
        if (m_freeList.compare_exchange_weak(oldHead, slot, std::memory_order_release, std::memory_order_relaxed)) {
            return true;
        }
    }
}

// 实现无锁出队操作
Slot* MemoryPool::popFreeList() {
    while(true) {
        Slot* oldHead = m_freeList.load(std::memory_order_relaxed);
        if (!oldHead) {
            return nullptr;
        }
        Slot* next = oldHead->next.load(std::memory_order_relaxed);
        if (m_freeList.compare_exchange_weak(oldHead, next, std::memory_order_acquire, std::memory_order_relaxed)) {
            return oldHead;
        }
    }
}

void HashBucket::initMemoryPool() {
    for (int i = 0; i < MEMORY_POOL_NUM; ++i) {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}

MemoryPool& HashBucket::getMemoryPool(int index) {
    static MemoryPool memoryPools[MEMORY_POOL_NUM];
    return memoryPools[index];
}



}  // namespace memoryPool