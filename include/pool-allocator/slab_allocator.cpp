#pragma once

#include <cstddef>
#include <memory>
#include <vector>

template <typename T, std::size_t SlabBlockCount>
class SlabAllocator
{
    static_assert(SlabBlockCount > 0, "Slab must hold at least one block ");

public:
    SlabAllocator() { add_slab(); }

    SlabAllocator(const SlabAllocator &) = delete;
    SlabAllocator &operator=(const SlabAllocator &) = delete;
    SlabAllocator(SlabAllocator &&) = delete;
    SlabAllocator &operator=(SlabAllocator &&) = delete;

    // Adds a new slab when the free list is empty,
    // so this can throw std::bad_alloc, unlike FixedBlockPool::allocate()

    [[nodiscard]] void *allocate()
    {
        if (free_list_ == nullptr)
        {
            add_slab();
        }

        Slot *slot = free_list_;
        free_list_ = free_list_->next;
        ++in_use_;
        return static_cast<void *>(slot);
    }

    void deallocate(void *ptr) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }
        Slot *slot = static_cast<Slot *>(ptr);
        slot->next = free_list_;
        free_list_ = slot;
        --in_use_;
    }

    std::size_t slab_count() const noexcept
    {
        return slabs_.size();
    }

    std::size_t in_use() const noexcept
    {
        return in_use_;
    }

    std::size_t capacity() const noexcept
    {
        return slabs_.size() * SlabBlockCount;
    }

private:
    union Slot
    {
        alignas(T) unsigned char storage[sizeof(T)];
        Slot *next;

        Slot() noexcept
        {
        }
        ~Slot() noexcept {}
    };
    static_assert(alignof(Slot) >= alignof(T),
                  "Slot must be aligned at least as strictly as T");

    struct Slab
    {
        std::unique_ptr<Slot[]> blocks;

        explicit Slab(std::size_t n) : blocks(new Slot[n]) {}
    };

    void add_slab()
    {
        slabs_.emplace_back(SlabBlockCount);
        Slot *base = slabs_.back().blocks.get();

        // Link each slot in the new slab to one after it.
        for (std::size_t i = 0; i + 1 < SlabBlockCount; ++i)
        {
            base[i].next = &base[i + 1];
        }

        // Links to the old head so free slots from
        // earlier slabs stay reachable.
        base[SlabBlockCount - 1].next = free_list_;
        free_list_ = base;
    }

    // Slabs hold only a pointer to their slots, so growing this vector
    // moves the Slab objects but never the slots themselves. Pointers
    // already returned by allocate() stay valid.
    std::vector<Slab> slabs_;
    Slot *free_list_ = nullptr;
    std::size_t in_use_ = 0;
};