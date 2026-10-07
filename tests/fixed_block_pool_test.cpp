#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "pool-allocator/fixed_block_pool.hpp"

struct OrderLevel
{
    std::int64_t price_ticks;
    std::uint64_t quantity;
    std::uint32_t order_count;
};

void new_pool_is_empty()
{
    FixedBlockPool<OrderLevel, 4> pool;
    assert(pool.capacity() == 4);
    assert(pool.in_use() == 0);
    assert(pool.available() == 4);
}

void allocates_until_full_then_returns_nullptr()
{
    FixedBlockPool<OrderLevel, 4> pool;

    void *blocks[4];
    for (std::size_t i = 0; i < 4; ++i)
    {
        blocks[i] = pool.allocate();
        assert(blocks[i] != nullptr);
    }

    assert(pool.in_use() == 4);
    assert(pool.available() == 0);

    void *extra = pool.allocate();
    assert(extra == nullptr);
}

void returns_distinct_pointers()
{
    FixedBlockPool<OrderLevel, 4> pool;

    void *a = pool.allocate();
    void *b = pool.allocate();
    void *c = pool.allocate();

    assert(a != b);
    assert(a != b);
    assert(b != c);
}

void freed_block_is_reused()
{
    FixedBlockPool<OrderLevel, 4> pool;

    void *first = pool.allocate();
    pool.deallocate(first);

    void *second = pool.allocate();
    assert(second == first);
    assert(pool.in_use() == 1);
}

void deallocating_nullptr_does_nothing()
{
    FixedBlockPool<OrderLevel, 4> pool;
    void *block = pool.allocate();
    assert(pool.in_use() == 1);

    pool.deallocate(nullptr);
    assert(pool.in_use() == 1);

    pool.deallocate(block);
    assert(pool.in_use() == 0);
}

struct alignas(64) CacheLineBlock
{
    char data[64];
};

void block_respect_alignment_of_t()
{
    FixedBlockPool<CacheLineBlock, 4> pool;

    for (int i = 0; i < 4; ++i)
    {
        void *block = pool.allocate();
        assert(block != nullptr);
        assert(reinterpret_cast<std::uintptr_t>(block) % alignof(CacheLineBlock) == 0);
    }
}
int main()
{
    new_pool_is_empty();
    allocates_until_full_then_returns_nullptr();
    returns_distinct_pointers;
    freed_block_is_reused;
    deallocating_nullptr_does_nothing();
    block_respect_alignment_of_t();
    std::puts("All test passed");
    return 0;
}