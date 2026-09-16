#pragma once

#include <cstdint>
#ifdef __APPLE__
#include <libproc.h>
#include <malloc/malloc.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

struct BenchmarkMemorySnapshot {
    int64_t footprint = -1;
    int64_t malloc_in_use = -1;
};

inline BenchmarkMemorySnapshot ReadBenchmarkMemory() {
    BenchmarkMemorySnapshot result;
#ifdef __APPLE__
    rusage_info_v4 usage{};
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t *>(&usage)) == 0)
        result.footprint = static_cast<int64_t>(usage.ri_phys_footprint);
    malloc_statistics_t allocations{};
    malloc_zone_statistics(nullptr, &allocations);
    result.malloc_in_use = static_cast<int64_t>(allocations.size_in_use);
#endif
    return result;
}
