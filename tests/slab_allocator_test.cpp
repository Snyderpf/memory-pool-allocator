#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>

#include "pool-allocator/slab_allocator.hpp"

struct OrderLevel
{
    std::int64_t price_ticks;
    std::uint64_t quantity;
    std::uint32_t order_count;
};

void new_allocator_has_one_slab()
{
    SlabAllocator<OrderLevel, 4> slab;

    assert(slab.slab_count() == 1);
    assert(slab.capacity() == 4);
    assert(slab.in_use() == 0);
}

void grows_when_first_slab_is_full()
{
    SlabAllocator<OrderLevel, 2> slab;

    void *a = slab.allocate();
    void *b = slab.allocate();
    assert(slab.slab_count() == 1);

    void *c = slab.allocate();
    assert(slab.slab_count() == 2);
    assert(slab.capacity() == 4);
    assert(slab.in_use() == 3);

    assert(a != nullptr && b != nullptr && c != nullptr);
    assert(a != b && b != c && a != c);
}

void pointers_stay_valid_after_growth()
{
    SlabAllocator<OrderLevel, 2> slab;

    void *raw = slab.allocate();
    OrderLevel *first = new (raw) OrderLevel{100, 50, 3};

    for (int i = 0; i < 100; ++i)
    {
        void *block = slab.allocate();
        assert(block != nullptr);
    }
    assert(slab.slab_count() == 51);

    assert(first->price_ticks == 100);
    assert(first->quantity == 50);
    assert(first->order_count == 3);
}

void freed_block_is_reused_after_growth()
{
    SlabAllocator<OrderLevel, 2> slab;

    void *a = slab.allocate();
    void *b = slab.allocate();
    void *c = slab.allocate();

    assert(b != nullptr && c != nullptr);
    assert(slab.slab_count() == 2);

    slab.deallocate(a);
    void *d = slab.allocate();

    assert(d == a);
    assert(slab.slab_count() == 2);
    assert(slab.in_use() == 3);
}

int main()
{
    new_allocator_has_one_slab();
    grows_when_first_slab_is_full();
    pointers_stay_valid_after_growth();
    freed_block_is_reused_after_growth();
    std::puts("all test passed");

    return 0;
}