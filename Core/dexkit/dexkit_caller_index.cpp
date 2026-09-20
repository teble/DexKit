// DexKit - An high-performance runtime parsing library for dex
// implemented in C++.
// Copyright (C) 2022-2023 LuckyPray
// https://github.com/LuckyPray/DexKit
//
// This program is free software: you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see
// <https://www.gnu.org/licenses/>.
// <https://github.com/LuckyPray/DexKit/blob/master/LICENSE>.

#include "dexkit.h"

#include "ThreadPool.h"

namespace dexkit {

// Internal segment consistency is checked in debug builds.
// Release validates the wide total before allocating each final payload;
// cursor bounds and the final consistency replay are debug-only.
void DexKit::BuildCompactCallers(uint32_t thread_num) {
    // Local counts and the original identity bindings are ready; no caller
    // payload has been allocated. Keep the original source/work-list order.
    for (auto &owner : dex_items)
        owner->method_caller_ids.BeginLayout(owner->method_invoking_ids.ValueCount());
    for (auto &owner : dex_items) {
        auto &source = owner->method_caller_ids;
        for (const auto &work : owner->pending_aggregate_method_work_items) {
            DEXKIT_CHECK(work.source_count != 0 && work.source_count == source.LocalCount(work.source_method_idx));
            source.EmptyRow(work.source_method_idx);
            dex_items[work.target_dex_id]->method_caller_ids.AddRowCount(work.target_method_idx, work.source_count);
        }
    }
    for (auto &owner : dex_items) owner->method_caller_ids.Allocate();

    // An unbound row first reserves its local prefix. It then serves as the
    // next-import cursor. A bound source row will instead hold its assigned
    // target segment's start. Count and cursor arrays are the same allocation.
    for (auto &owner : dex_items) {
        auto &index = owner->method_caller_ids;
        for (uint32_t method = 0; method < index.size(); ++method) {
            if (!owner->method_cross_info[method]) {
                DEXKIT_CHECK(uint64_t(index.RowBegin(method)) + index.LocalCount(method) <= index.RowEnd(method));
                index.Cursor(method) = index.RowBegin(method) + index.LocalCount(method);
            } else {
                DEXKIT_CHECK(index.RowBegin(method) == index.RowEnd(method));
            }
        }
    }
    for (auto &owner : dex_items) {
        for (const auto &work : owner->pending_aggregate_method_work_items) {
            auto &target = dex_items[work.target_dex_id]->method_caller_ids;
            auto &next = target.Cursor(work.target_method_idx);
            owner->method_caller_ids.Cursor(work.source_method_idx) = next;
            DEXKIT_CHECK(uint64_t(next) + work.source_count <= target.RowEnd(work.target_method_idx));
            next += work.source_count;
        }
    }
    for (auto &owner : dex_items) {
        auto &index = owner->method_caller_ids;
        for (uint32_t method = 0; method < index.size(); ++method) {
            if (!owner->method_cross_info[method]) {
                DEXKIT_CHECK(index.Cursor(method) == index.RowEnd(method));
                index.Cursor(method) = index.RowBegin(method);
            }
        }
    }

    const auto fill_source = [this](DexItem *source) {
        auto &index = source->method_caller_ids;
        for (const auto &definition : source->reader.ClassDefs()) {
            for (auto caller : source->class_method_ids[definition.class_idx]) {
                for (auto invoked : source->method_invoking_ids[caller]) {
                    auto &cursor = index.Cursor(invoked);
                    const auto &binding = source->method_cross_info[invoked];
                    auto &target = binding ? dex_items[binding.value().first]->method_caller_ids : index;
                    target.Write(cursor++, static_cast<uint16_t>(source->dex_id), caller);
                }
            }
        }
    };
    if (thread_num > 1 && dex_items.size() > 1) {
        ThreadPool pool(std::min(static_cast<size_t>(thread_num), dex_items.size()));
        for (auto &owner : dex_items) pool.enqueue(fill_source, owner.get());
    } else {
        for (auto &owner : dex_items) fill_source(owner.get());
    }

#if !defined(NDEBUG)
    // Local cursors now point just past their local prefixes. Replaying the
    // same imports checks each source's final cursor against its expected end,
    // then checks every final row end before any consumer can see the payload.
    for (auto &owner : dex_items) {
        for (const auto &work : owner->pending_aggregate_method_work_items) {
            auto &target = dex_items[work.target_dex_id]->method_caller_ids;
            auto &end = target.Cursor(work.target_method_idx);
            DEXKIT_CHECK(uint64_t(end) + work.source_count <= target.RowEnd(work.target_method_idx));
            end += work.source_count;
            DEXKIT_CHECK(owner->method_caller_ids.Cursor(work.source_method_idx) == end);
        }
    }
    for (auto &owner : dex_items) {
        auto &index = owner->method_caller_ids;
        for (uint32_t method = 0; method < index.size(); ++method)
            if (!owner->method_cross_info[method]) DEXKIT_CHECK(index.Cursor(method) == index.RowEnd(method));
    }
#endif
    for (auto &owner : dex_items) {
        owner->method_caller_ids.ReleaseBuild();
        decltype(owner->pending_aggregate_method_work_items)().swap(owner->pending_aggregate_method_work_items);
    }
    // The caller returns to FinishBuildCrossRefAggregates for publication.
}

} // namespace dexkit
