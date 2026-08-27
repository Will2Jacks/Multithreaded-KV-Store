#pragma once
#include <cstddef>
#include <mutex>
#include <vector>
#include <new>
#include <stdexcept>

// Global Memory Manager to hold the raw 1GB block
class SlabManager {
    private:
        static constexpr size_t CHUNK_SIZE = 128; 
        static constexpr size_t POOL_SIZE = 1024 * 1024 * 1024; // 1GB
        
        char* memory_block;
        std::vector<void*> free_list;
        std::mutex mtx;

        SlabManager() 
        {
            memory_block = static_cast<char*>(operator new(POOL_SIZE));
            size_t num_chunks = POOL_SIZE / CHUNK_SIZE;
            free_list.reserve(num_chunks);
            
            // Slice the 1GB block into fixed-size chunks and add to the free list
            for(size_t i = 0; i < num_chunks; ++i) 
            {
                free_list.push_back(memory_block + (i * CHUNK_SIZE));
            }
        }

        ~SlabManager() 
        {
            operator delete(memory_block);
        }

    public:
        // Singleton pattern ensures all shards share the exact same 1GB pool
        static SlabManager& get_instance() 
        {
            static SlabManager instance;
            return instance;
        }

        void* allocate(size_t n) 
        {
            // If standard map nodes exceed chunk size, gracefully fallback to OS malloc
            if(n > CHUNK_SIZE) return operator new(n); 
            
            std::lock_guard<std::mutex> lock(mtx);
            if(free_list.empty()) throw std::bad_alloc();
            
            void* ptr = free_list.back();
            free_list.pop_back();
            return ptr;
        }

        void deallocate(void* p, size_t n) 
        {
            if(n > CHUNK_SIZE) 
            {
                operator delete(p);
                return;
            }
            
            std::lock_guard<std::mutex> lock(mtx);
            free_list.push_back(p);
        }
};

// The STL-compliant Allocator Template
template <typename T>
struct SlabAllocator {
    using value_type = T;

    SlabAllocator() noexcept {}
    template <typename U> SlabAllocator(const SlabAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) 
    {
        return static_cast<T*>(SlabManager::get_instance().allocate(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept 
    {
        SlabManager::get_instance().deallocate(p, n * sizeof(T));
    }

    // Required by C++ standard for custom allocators
    bool operator==(const SlabAllocator&) const noexcept { return true; }
    bool operator!=(const SlabAllocator&) const noexcept { return false; }
};