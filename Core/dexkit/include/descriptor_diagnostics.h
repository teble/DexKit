#pragma once

#if DEXKIT_BENCHMARK_DIAGNOSTICS
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace dexkit {
enum class DescriptorUse : size_t { Output, CrossReference, Lookup };
inline thread_local DescriptorUse current_descriptor_use = DescriptorUse::Output;

struct DescriptorUseScope {
    DescriptorUse previous = current_descriptor_use;
    explicit DescriptorUseScope(DescriptorUse use) { current_descriptor_use = use; }
    ~DescriptorUseScope() { current_descriptor_use = previous; }
};

struct DescriptorDiagnostics {
    std::array<std::atomic<uint64_t>, 3> method_calls{};
    std::array<std::atomic<uint64_t>, 3> field_calls{};
    std::array<std::atomic<uint64_t>, 3> method_builds{};
    std::array<std::atomic<uint64_t>, 3> field_builds{};
    std::array<std::atomic<uint64_t>, 3> materialized_bytes{};
    std::atomic<uint64_t> method_comparisons{0};
    std::atomic<uint64_t> field_comparisons{0};

    void Called(bool method) {
        const auto use = static_cast<size_t>(current_descriptor_use);
        (method ? method_calls : field_calls)[use].fetch_add(1, std::memory_order_relaxed);
    }

    void Built(bool method, size_t bytes) {
        const auto use = static_cast<size_t>(current_descriptor_use);
        (method ? method_builds : field_builds)[use].fetch_add(1, std::memory_order_relaxed);
        materialized_bytes[use].fetch_add(bytes, std::memory_order_relaxed);
    }
};
} // namespace dexkit
#endif
