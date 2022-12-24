#include <benchmark/benchmark.h>

#include "wordpiece.hpp"

static void BM_simple(benchmark::State &state) {
    for (auto _ : state) {
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_simple);
BENCHMARK_MAIN();
