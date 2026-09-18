#include "compact_caller_index.h"

#include <cstdio>
#include <csignal>
#include <limits>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
using Index = dexkit::CompactCallerIndex;
void Require(bool value) { if (!value) std::abort(); }
#ifndef _WIN32
template<class Body> void Aborts(Body body) {
    auto child = fork();
    Require(child >= 0);
    if (child == 0) { body(); _exit(0); }
    int status = 0;
    Require(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}
#endif
}

int main() {
    Index empty;
    empty.BeginCounts(0);
    empty.BeginLayout();
    empty.Allocate();
    empty.ReleaseBuild();
    Require(empty.empty() && empty.size() == 0 && empty.BuildCapacityBytes() == 0);

    Index rows;
    rows.BeginCounts(5);
    rows.Count(3); rows.Count(1); rows.Count(3);
    rows.BeginLayout();
    rows.EmptyRow(1);
    rows.AddRowCount(4, 1);
    rows.Allocate();
    Require(rows.size() == 5 && rows[0].empty() && rows[1].empty() && rows[2].empty());
    Require(rows[3].size() == 2 && rows[4].size() == 1);
    rows.Write(rows.RowBegin(3), 0, 0);
    constexpr auto largest_method = std::numeric_limits<dexkit::LocalMethodId>::max();
    rows.Write(rows.RowBegin(3) + 1, UINT16_MAX, largest_method);
    rows.Write(rows.RowBegin(4), 7, 19);
    auto held = rows[3];
    rows.ReleaseBuild();
    Require(rows.BuildCapacityBytes() == 0 && held.data() == rows[3].data());
    Require(held[0] == Index::Entry{0, 0} && held[1] == Index::Entry{UINT16_MAX, largest_method});
    Require(rows[4][0] == Index::Entry{7, 19});
    const auto limit = std::numeric_limits<dexkit::CacheOffset>::max();
    Require(Index::CheckedAdd(limit - 1, 1) == limit && Index::CheckedAdd(limit, 0) == limit);
#ifndef _WIN32
    Aborts([&] { Index::CheckedAdd(limit, 1); });
    Aborts([&] { Index invalid; invalid.BeginCounts(std::numeric_limits<size_t>::max()); });
    Aborts([&] { rows.Write(3, 0, 0); });
    Aborts([&] { (void)rows[5]; });
    Aborts([&] { Index invalid; invalid.BeginCounts(1); invalid.Count(1); });
#if DEXKIT_EXPERIMENT_NARROW_TYPES
    Aborts([&] { rows.Write(0, 0, uint32_t{UINT16_MAX} + 1); });
    Aborts([&] {
        Index invalid; invalid.BeginCounts(1); invalid.BeginLayout();
        invalid.AddRowCount(0, limit); invalid.AddRowCount(0, 1);
    });
#else
    Aborts([&] {
        Index invalid; invalid.BeginCounts(1); invalid.BeginLayout();
        invalid.AddRowCount(0, limit / sizeof(Index::Entry) + 1); invalid.Allocate();
    });
#endif
#endif
    std::puts("CHECK_CALLER_INDEX {\"empty\":true,\"boundaries\":true,\"stable\":true,\"released\":true}");
}
