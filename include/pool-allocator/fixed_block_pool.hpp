#pragma once

#include <cstddef>
#include <cstdint>

template <typename T, std::size_t BlockCount>
class FixedBlockPool
{
    static_assert(BlockCount > 0, "Pool must hold at least one block");

    union Slot
    {
        alignas(T) unsigned char storage[sizeof(T)];
        Slot *next;

        Slot() noexcept {}
        ~Slot() noexcept {}
    };

public:
    FixedBlockPool() noexcept
    {
        for (std::size_t i = 0; i + 1 < BlockCount; ++i)
        {
            arena_[i].next = &arena_[i + 1];
        }

        arena_[BlockCount - 1].next = nullptr;
        free_list_ = &arena_[0];
    }

    // Not copyable. The arena is a fixed array and allocate()
    // hands out pointers to it , so copying the pool would leave
    // those pointers aimed at the original, not the copy.
    // Not movale yet keeping the first version simple

    FixedBlockPool(const FixedBlockPool &) = delete;
    FixedBlockPool &operator=(const FixedBlockPool &) = delete;
    FixedBlockPool(FixedBlockPool &&) = delete;
    FixedBlockPool &operator=(FixedBlockPool &&) = delete;

    [[nodiscard]] void *allocate() noexcept
    {
        if (free_list_ == nullptr)
        {
            return nullptr;
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

    std::size_t capacity() const noexcept
    {
        return BlockCount;
    }
    std::size_t in_use() const noexcept
    {
        return in_use_;
    }
    std::size_t available() const noexcept
    {
        return BlockCount - in_use_;
    }

private:
    Slot arena_[BlockCount];
    Slot *free_list_ = nullptr;
    std::size_t in_use_ = 0;
};