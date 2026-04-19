#pragma once

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>


namespace memoryPool
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

struct Slot {
    std::atomic<Slot*> next;
};

class MemoryPool {
public:
    MemoryPool(size_t blockSize = 4096);
    ~MemoryPool();

    void init(size_t size);

    void* allocate();
    void deallocate(void* ptr);

private:
    void allocateNewBlock();
    size_t padPointer(char* ptr, size_t alignment);

    bool pushFreeList(Slot* slot);
    Slot* popFreeList();
    
private:
    int                m_blockSize;        // 内存块大小
    int                m_slotSize;         // 槽大小
    Slot*              m_curSlot;          // 指向当前未被使用的槽
    Slot*              m_firstBlock;       // 指向内存池管理的首个实际内存块
    Slot*              m_lastSlot;         // 作为当前内存池中最后能够存放元素的位置标识
    std::atomic<Slot*> m_freeList;         // 指向空闲的槽（被使用后又被释放的槽）
    std::mutex         m_mutexForCurSlot;  // 保护curSlot的互斥锁
};

class HashBucket {
public:
    static void initMemoryPool();
    static MemoryPool& getMemoryPool(int index);

    static void* useMemory(size_t size) {
        if (size <= 0) {
            return nullptr;
        }
        if (size > MAX_SLOT_SIZE) {
            return operator new(size);
        }
        return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
    }

    static void freeMemory(void* ptr, size_t size) {
        if (!ptr) {
            return ;
        }
        if (size > MAX_SLOT_SIZE) {
            operator delete(ptr);
            return ;
        }
        getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
    }

    template<typename T, typename...Args>
    T* newElement(Args&&... args);

    template<typename T>
    void deleteElement(T* p);
};

template<typename T, typename...Args>
T* newElement(Args&&... args) {
    T* p = nullptr;
    // 根据元素大小选择合适的内存池分配内存
    if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr) {
        // 在分配的内存上构造对象
        new(p) T(std::forward<Args>(args)...);
    }
    return p;
}

template<typename T>
void deleteElement(T* p) {
    if (p) {
        p->~T();
        // 内存回收
        HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
    }
}

}  // namespace memoryPool