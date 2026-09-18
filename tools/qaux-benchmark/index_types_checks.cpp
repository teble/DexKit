#include "compact_caller_index.h"
#include "compact_field_index.h"
#include "compact_string_index.h"

#include <csignal>
#include <cstdio>
#include <limits>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
using namespace dexkit;
void Require(bool condition) { if (!condition) std::abort(); }
#ifndef _WIN32
template<class Body> void Aborts(Body body) {
    const auto child = fork();
    Require(child >= 0);
    if (child == 0) { body(); _exit(0); }
    int status = 0;
    Require(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}
#endif
}

int main() {
    constexpr size_t method_count = size_t{UINT16_MAX} + 1;
    constexpr size_t long_row = 70000;
    Require(CheckedIndexCast<LocalMethodId>(UINT16_MAX) == UINT16_MAX);
    Require(CheckedIndexCast<ClassDefIndex>(UINT16_MAX) == UINT16_MAX);

    CompactInvocationIndex invokes;
    invokes.resize(method_count);
    // Method traversal is not ID ordered; duplicate IDs and long lengths survive.
    auto *ids = invokes.BeginMethod(UINT16_MAX);
    for (size_t i = 0; i < long_row; ++i) ids->push_back(i % 2 ? UINT16_MAX : 0);
    invokes.EndMethod(UINT16_MAX);
    auto *first = invokes.BeginMethod(0);
    first->push_back(7); first->push_back(12); first->push_back(7);
    invokes.EndMethod(0);
    Require(invokes[1].empty() && invokes[UINT16_MAX].size() == long_row);
    Require(invokes[0].size() == 3 && invokes[0][0] == 7 && invokes[0][1] == 12 && invokes[0][2] == 7);
    for (size_t i = 0; i < long_row; ++i) Require(invokes[UINT16_MAX][i] == (i % 2 ? UINT16_MAX : 0));

    CompactStringIndex strings;
    strings.resize(2);
    auto *string_ids = strings.BeginMethod(1);
    for (uint32_t id : {uint32_t{0}, uint32_t{65535}, uint32_t{65536}, UINT32_MAX}) string_ids->push_back(id);
    strings.EndMethod(1);
    Require(strings[0].empty() && strings[1].size() == 4 && strings[1][2] == 65536 && strings[1][3] == UINT32_MAX);

    CompactFieldIndex fields;
    fields.resize(2);
    auto *uses = fields.BeginMethod(1);
    uses->emplace_back(UINT16_MAX, true); uses->emplace_back(UINT16_MAX, false);
    fields.EndMethod(1);
    Require(fields[0].empty() && fields[1].size() == 2);
    Require(DecodeFieldUse(fields[1][0]) == std::pair<uint32_t, bool>{UINT16_MAX, true});
    Require(DecodeFieldUse(fields[1][1]) == std::pair<uint32_t, bool>{UINT16_MAX, false});

    CompactCallerIndex callers;
    callers.BeginCounts(method_count);
    for (size_t i = 0; i < long_row; ++i) callers.Count(UINT16_MAX);
    callers.BeginLayout(); callers.Allocate();
    for (size_t i = 0; i < long_row; ++i) callers.Write(callers.RowBegin(UINT16_MAX) + i, UINT16_MAX, UINT16_MAX);
    auto held = callers[UINT16_MAX];
    callers.ReleaseBuild();
    Require(held.size() == long_row && held.data() == callers[UINT16_MAX].data());
    Require(callers[0].empty() && callers.BuildCapacityBytes() == 0);
    for (auto [dex, method] : held) Require(dex == UINT16_MAX && method == UINT16_MAX);

    const auto limit = std::numeric_limits<CacheOffset>::max();
    Require(CheckedOffsetAdd(limit - 1, 1) == limit);
    Require(CheckedOffsetAdd(limit, 0) == limit);
#ifndef _WIN32
    Aborts([&] { CheckedOffsetAdd(limit, 1); });
    Aborts([&] { CheckedIndexCast<uint16_t>(uint32_t{UINT16_MAX} + 1); });
    Aborts([&] { CheckedIndexCast<uint32_t>(uint64_t{UINT32_MAX} + 1); });
    Aborts([&] { CheckedIndexCast<uint32_t>(-1); });
    Aborts([&] {
        CompactCallerIndex overflow;
        overflow.BeginCounts(2); overflow.BeginLayout();
        overflow.AddRowCount(0, limit); overflow.AddRowCount(1, 1);
        // Each row fits, but their prefix sum must fail before payload allocation.
        overflow.Allocate();
    });
#if DEXKIT_EXPERIMENT_NARROW_TYPES
    if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
        Aborts([&] { CompactCallerIndex::CheckedAdd(static_cast<size_t>(uint64_t{UINT32_MAX} + 1), 0); });
    }
#endif
#endif
    std::printf("CHECK_INDEX_TYPES {\"method_bytes\":%zu,\"invoke_bytes\":%zu,\"offset_bytes\":%zu,\"caller_bytes\":%zu,"
                "\"rw_bytes\":%zu,\"class_def_bytes\":%zu,\"max_id\":65535,\"long_row\":70000,\"string_ids_32bit\":true}\n",
                sizeof(LocalMethodId), sizeof(InvokeOperandId), sizeof(CacheOffset), sizeof(CompactCallerIndex::Entry),
                sizeof(MethodReference), sizeof(ClassDefIndex));
}
