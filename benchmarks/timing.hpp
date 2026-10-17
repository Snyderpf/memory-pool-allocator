#pragma once

#include <cstdint>
#include <x86intrin.h>

// Reads the CPU's cycle counter. The fences keep the CPU from
// moving other instructions across the read, so the number reflects
// only the code place between two calls.
inline std::uint64_t read_cycles()
{
    unsigned int aux;
    _mm_lfence();
    std::uint64_t cycles = __rdtscp(&aux);
    _mm_lfence();

    return cycles;
}