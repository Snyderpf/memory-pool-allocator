#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <numeric>
#include <random>

#include "pool-allocator/fixed_block_pool.hpp"
#include "pool-allocator/slab_allocator.hpp"
#include "timing.hpp"

struct Stats
{
    std::uint64_t p50;
    std::uint64_t p95;
    std::uint64_t p99;
    std::uint64_t p999;
    std::uint64_t max;
};

// Sorts the samples in place, so the timeline order is lost
Stats compute_stats(std::vector<std::uint64_t> &samples)
{
    std::sort(samples.begin(), samples.end());

    auto at = [&](double fraction)
    {
        return samples[static_cast<std::size_t>(fraction * (samples.size() - 1))];
    };

    return Stats{at(0.50), at(0.95), at(0.99), at(0.999), samples.back()};
}

constexpr std::size_t kOps = 10'000'000;

// Cost of one measurement with nothing between two reads. Every
// allocator sample includes this, so it is reported next to the results.
Stats measure_timer_overhead()
{
    std::vector<std::uint64_t> samples;
    samples.reserve(kOps);

    for (std::size_t i = 0; i < kOps; ++i)
    {
        std::uint64_t start = read_cycles();
        std::uint64_t end = read_cycles();
        samples.push_back(end - start);
    }

    return compute_stats(samples);
}

struct OrderLevel
{
    std::int64_t price_ticks;
    std::uint64_t quantity;
    std::uint32_t order_count;
};

// Stops the compiler from deleting an allocation
// whose result looks unused.
inline void keep(void *p)
{
    asm volatile("" : : "r"(p) : "memory");
}

struct MallocAllocator
{
    void *allocate()
    {
        return std::malloc(sizeof(OrderLevel));
    }

    void deallocate(void *p)
    {
        std::free(p);
    }
};

template <typename Allocator>
std::vector<std::uint64_t> bench_allocator(Allocator &allocator)
{
    std::vector<std::uint64_t> samples;
    samples.reserve(kOps);

    for (std::size_t i = 0; i < kOps; ++i)
    {
        std::uint64_t start = read_cycles();
        void *p = allocator.allocate();
        keep(p);
        std::uint64_t end = read_cycles();

        allocator.deallocate(p);
        samples.push_back(end - start);
    }
    return samples;
}

constexpr std::size_t kBatch = 5000;

template <typename Allocator>
std::vector<std::uint64_t> bench_batch(Allocator &allocator)
{
    std::vector<std::uint64_t> samples;
    samples.reserve(kOps);
    std::vector<void *> live(kBatch);
    std::vector<std::size_t> order(kBatch);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(12345);

    for (std::size_t round = 0; round < kOps / kBatch; ++round)
    {
        for (std::size_t i = 0; i < kBatch; ++i)
        {
            std::uint64_t start = read_cycles();
            void *p = allocator.allocate();
            keep(p);
            std::uint64_t end = read_cycles();
            live[i] = p;
            samples.push_back(end - start);
        }

        std::shuffle(order.begin(), order.end(), rng);
        for (std::size_t i : order)
        {
            allocator.deallocate(live[i]);
        }
    }
    return samples;
}

void print_row(const char *name, const Stats &s)
{
    std::printf("%-14s %8llu %8llu %8llu %8llu %10llu\n", name, (unsigned long long)s.p50, (unsigned long long)s.p95,
                (unsigned long long)s.p99, (unsigned long long)s.p999,
                (unsigned long long)s.max);
}

// Writes the sorted samples at 1000 evenly spaced ranks, then a few
// extra points deep in the tail, which the chart needs to show.
void append_distribution(std::FILE *file, const char *name,
                         const std::vector<std::uint64_t> &sorted)
{
    auto write_point = [&](double fraction)
    {
        std::size_t index = static_cast<std::size_t>(fraction * (sorted.size() - 1));
        std::fprintf(file, "%s,%.4f,%llu\n", name, fraction * 100.0,
                     (unsigned long long)sorted[index]);
    };

    for (std::size_t i = 0; i < 1000; ++i)
    {
        write_point(static_cast<double>(i) / 1000.0);
    }

    const double tail[] = {0.9995, 0.9999, 0.99999, 0.999999, 1.0};
    for (double fraction : tail)
    {
        write_point(fraction);
    }
}

int main()
{
    std::FILE *csv = std::fopen("results/latency_distribution.csv", "w");
    if (csv == nullptr)
    {
        std::perror("results/latency_distribution.csv");
        return 1;
    }

    std::FILE *csv_batch = std::fopen("results/latency_distribution_batch.csv", "w");
    if (csv_batch == nullptr)
    {
        std::perror("results/latency_distribution_batch.csv");
        return 1;
    }

    Stats overhead = measure_timer_overhead();

    MallocAllocator malloc_allocator;
    FixedBlockPool<OrderLevel, 1024> pool;
    FixedBlockPool<OrderLevel, kBatch> pool_bench;
    SlabAllocator<OrderLevel, 1024> slab;

    std::vector<std::uint64_t> malloc_samples = bench_allocator(malloc_allocator);
    std::vector<std::uint64_t> pool_samples = bench_allocator(pool);
    std::vector<std::uint64_t> slab_samples = bench_allocator(slab);

    std::vector<std::uint64_t> malloc_bench_samples = bench_batch(malloc_allocator);
    std::vector<std::uint64_t> pool_bench_samples = bench_batch(pool_bench);
    std::vector<std::uint64_t> slab_bench_samples = bench_batch(slab);

    Stats malloc_stats = compute_stats(malloc_samples);
    Stats pool_stats = compute_stats(pool_samples);
    Stats slab_stats = compute_stats(slab_samples);

    Stats malloc_bench_stats = compute_stats(malloc_bench_samples);
    Stats pool_bench_stats = compute_stats(pool_bench_samples);
    Stats slab_bench_stats = compute_stats(slab_bench_samples);

    std::printf("%zu operations per allocator, times in CPU cycles\n\n", kOps);
    std::printf("%-14s %8s %8s %8s %8s %10s\n", "", "p50", "p95", "p99", "p99.9", "max");
    print_row("timer only", overhead);
    print_row("malloc", malloc_stats);
    print_row("FixedBlockPool", pool_stats);
    print_row("SlabAllocator", slab_stats);

    std::fprintf(csv, "allocator,percentile,cycles\n");
    append_distribution(csv, "malloc", malloc_samples);
    append_distribution(csv, "FixedBlockPool", pool_samples);
    append_distribution(csv, "SlabAllocator", slab_samples);

    std::printf("batch of %zu live blocks,shuffled frees, times in CPU cycles\n\n", kBatch);
    std::printf("%-14s %8s %8s %8s %8s %10s\n", "", "p50", "p95", "p99", "p99.9", "max");
    print_row("timer only", overhead);
    print_row("malloc", malloc_bench_stats);
    print_row("FixedBlockPool", pool_bench_stats);
    print_row("SlabAllocator", slab_bench_stats);

    std::fprintf(csv_batch, "allocator,percentile,cycles\n");
    append_distribution(csv_batch, "malloc", malloc_bench_samples);
    append_distribution(csv_batch, "FixedBlockPool", pool_bench_samples);
    append_distribution(csv_batch, "SlabAllocator", slab_bench_samples);

    std::fclose(csv);
    std::fclose(csv_batch);

    return 0;
}