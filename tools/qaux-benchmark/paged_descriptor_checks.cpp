#include "paged_descriptor_cache.h"
#include <atomic>
#include <cstdio>
#include <latch>
#include <string>
#include <thread>

namespace {
void Require(bool value, const char *message) {
    if (!value) { std::fprintf(stderr, "Paged descriptor check failed: %s\n", message); std::abort(); }
}
std::string_view Put(dexkit::PagedDescriptorCache &cache, size_t index, std::string_view text) {
    return cache.GetOrCreate(index, [&](auto &&append) {
        append(text.substr(0, text.size() / 2));
        append({});
        append(text.substr(text.size() / 2));
    }, [](size_t) {});
}
void Check(std::string_view view, std::string_view text) {
    Require(view == text, "complete body bytes");
    Require(view.data()[view.size()] == '\0', "trailing NUL");
#if DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS
    Require(reinterpret_cast<uintptr_t>(view.data()) % 16 == 0, "body address aligned to 16 bytes");
#endif
}
}

int main(int argc, char **argv) {
    dexkit::PagedDescriptorCache empty;
    empty.Initialize(0);
    Require(empty.size() == 0 && empty.GetStatistics().records == 0, "empty cache");
    if (argc == 2 && std::string_view(argv[1]) == "invalid-index") {
        Put(empty, 0, "invalid");
        return 1;
    }
    if (argc == 2 && std::string_view(argv[1]) == "repeat-initialize") {
        empty.Initialize(0);
        return 1;
    }

    dexkit::PagedDescriptorCache exact, over;
    exact.Initialize(2);
    over.Initialize(2);
    const std::string fill(4096 - sizeof(size_t) - 1, 'e');
    Check(Put(exact, 0, fill), fill);
#if DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS
    Require(exact.GetStatistics().record_used_bytes >= 4096
            && exact.GetStatistics().record_used_bytes <= 4096 + 15
            && exact.GetStatistics().record_capacity_bytes == 4096 + 15, "initial boundary plus alignment slack");
    Check(Put(exact, 1, ""), "");
#else
    Require(exact.GetStatistics().record_used_bytes == 4096
            && exact.GetStatistics().record_capacity_bytes == 4096, "record exactly fills initial block");
    Check(Put(exact, 1, ""), "");
    Require(exact.GetStatistics().blocks == 2, "next record allocates a new block");
#endif
    const std::string overflow(fill.size() + 1, 'o');
    Check(Put(over, 0, overflow), overflow);
#if DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS
    Require(over.GetStatistics().record_used_bytes >= 4097
            && over.GetStatistics().record_used_bytes <= 4097 + 15
            && over.GetStatistics().record_capacity_bytes == 4097 + 15, "oversized record includes alignment slack");
#else
    Require(over.GetStatistics().record_used_bytes == 4097
            && over.GetStatistics().record_capacity_bytes == 4097, "record exceeds initial block by one byte");
#endif

    constexpr size_t count = 2305;
    dexkit::PagedDescriptorCache cache;
    cache.Initialize(count);
    Require(!cache.TryGet(0), "unallocated page cache miss");
    const std::string original("stable\0utf8-\xce\xa9", 14);
    const auto retained = Put(cache, 0, original);
    Require(!cache.TryGet(1), "unfilled slot cache miss");
    Check(*cache.TryGet(0), original);
    const auto *address = retained.data();
    const std::string large(65536 + 83, 'L');
    Check(Put(cache, 255, large), large);
    Check(Put(cache, 256, "next page"), "next page");
    Check(Put(cache, 2048, "same stripe, next page"), "same stripe, next page");

    std::latch started(1);
    std::atomic<bool> done{false};
    std::atomic<uint64_t> reads{0};
    std::thread reader([&] {
        started.count_down();
        do {
            Check(retained, original);
            Require(retained.data() == address, "retained address");
            Check(Put(cache, 0, "must not replace published bytes"), original);
            Check(*cache.TryGet(0), original);
            reads.fetch_add(1, std::memory_order_relaxed);
        } while (!done.load(std::memory_order_acquire));
    });
    started.wait();
    while (reads.load(std::memory_order_relaxed) == 0) std::this_thread::yield();
    const auto before_growth = reads.load(std::memory_order_relaxed);
    for (size_t i = 1; i < count; ++i) {
        if (i == 255 || i == 256 || i == 2048) continue;
        std::string text(i % 65, char('a' + i % 26));
        Check(Put(cache, i, text), text);
    }
    while (reads.load(std::memory_order_relaxed) == before_growth) std::this_thread::yield();
    done.store(true, std::memory_order_release);
    reader.join();
    Require(cache.GetStatistics().records == count && cache.GetStatistics().pages == 10,
            "every slot and page boundary populated once");
    Check(retained, original);
    std::puts("CHECK_PAGED_DESCRIPTORS {\"records\":2305,\"passed\":true}");
}
