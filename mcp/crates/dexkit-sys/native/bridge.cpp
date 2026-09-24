// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "bridge.h"
#include "dexkit.h"
#include "dex_item.h"
#include "schema/querys_generated.h"
#include "schema/results_generated.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
constexpr size_t kMaxResult = 64u * 1024 * 1024;
[[noreturn]] void FatalException() noexcept {
    // Core is not proven reusable after an unexpected exception (for example,
    // during warmup). Production invokes this ABI in a dedicated worker.
    std::fputs("DexKit native exception: worker state invalidated\n", stderr);
    std::fflush(stderr);
    std::_Exit(70);
}
struct Context {
    dexkit::DexKit core;
};
int Copy(const uint8_t *data, size_t size, DkBuffer *output) {
    if (size > kMaxResult) return 5;
    auto owned = std::make_unique<uint8_t[]>(size);
    if (size) std::memcpy(owned.get(), data, size);
    output->size = size;
    output->data = owned.release();
    return 0;
}
template<class T> const T *Query(const uint8_t *data, size_t size) {
    if (!data || !size || size > 1024 * 1024) return nullptr;
    flatbuffers::Verifier verifier(data, size, 64, 100000);
    return verifier.VerifyBuffer<T>(nullptr) ? flatbuffers::GetRoot<T>(data) : nullptr;
}
bool ValidId(Context& context, uint64_t id, unsigned kind) {
    const auto dex_id = id >> 32;
    if (dex_id >= uint64_t(context.core.GetDexNum()) || dex_id > UINT16_MAX) return false;
    const auto item = context.core.GetDexItem(static_cast<uint16_t>(dex_id));
    const auto index = static_cast<uint32_t>(id);
    return kind == 1 ? item->HasTypeId(index) : kind == 2 ? item->HasMethodId(index) : item->HasFieldId(index);
}
} // namespace

extern "C" uint32_t dk_default_thread_count() {
    try { return dexkit::DexKit().GetThreadNum(); }
    catch (...) { FatalException(); }
}
extern "C" int dk_open(const char *path, uint64_t max_dex_bytes, uint32_t threads,
    void **opaque, uint32_t *count) {
    if (!opaque || !count) return 1;
    *opaque = nullptr; *count = 0;
    if (!path || threads > INT32_MAX) return 1;
    try {
        auto ctx = std::make_unique<Context>();
        if (threads) ctx->core.SetThreadNum(static_cast<int>(threads));
        auto result = ctx->core.AddPath(path, max_dex_bytes);
        if (result == dexkit::Error::DEX_BYTES_EXCEEDED) return 7;
        if (result != dexkit::Error::SUCCESS) return 1;
        *count = ctx->core.GetDexNum();
        if (!*count) return 1;
        if (*count > UINT16_MAX) return 5;
        *opaque = ctx.release();
        return 0;
    } catch (...) { FatalException(); }
}
extern "C" void dk_close(void *opaque) { delete static_cast<Context *>(opaque); }
extern "C" void dk_free(DkBuffer buffer) { delete[] buffer.data; }

extern "C" int dk_call(void *opaque, uint32_t op, uint64_t id,
    const uint8_t *query, size_t size, DkBuffer *out) {
    if (!out) return 1;
    *out = {};
    if (!opaque) return 1;
    auto &ctx = *static_cast<Context *>(opaque);
    using namespace dexkit::schema;
    try {
        std::unique_ptr<flatbuffers::FlatBufferBuilder> result;
        switch (op) {
            case 1: { auto q = Query<FindClass>(query,size); if (!q) return 2; result = ctx.core.FindClass(q); break; }
            case 2: { auto q = Query<FindMethod>(query,size); if (!q) return 2; result = ctx.core.FindMethod(q); break; }
            case 3: { auto q = Query<FindField>(query,size); if (!q) return 2; result = ctx.core.FindField(q); break; }
            case 4: if (!ValidId(ctx, id, 1)) return 3; result = ctx.core.GetClassAnnotations(id); break;
            case 5: if (!ValidId(ctx, id, 2)) return 3; result = ctx.core.GetMethodAnnotations(id); break;
            case 6: if (!ValidId(ctx, id, 3)) return 3; result = ctx.core.GetFieldAnnotations(id); break;
            case 7: if (!ValidId(ctx, id, 2)) return 3; result = ctx.core.GetUsingNumbers(id); break;
            case 8: if (!ValidId(ctx, id, 2)) return 3; result = ctx.core.GetInvokeMethods(id); break;
            case 9: if (!ValidId(ctx, id, 2)) return 3; result = ctx.core.GetCallMethods(id); break;
            case 10: if (!ValidId(ctx, id, 3)) return 3; result = ctx.core.FieldGetMethods(id); break;
            case 11: if (!ValidId(ctx, id, 3)) return 3; result = ctx.core.FieldPutMethods(id); break;
            case 12: {
                if (!ValidId(ctx, id, 2)) return 3;
                auto strings = ctx.core.GetUsingStrings(id);
                result = std::make_unique<flatbuffers::FlatBufferBuilder>();
                std::vector<flatbuffers::Offset<flatbuffers::String>> offsets;
                for (auto text : strings) offsets.push_back(result->CreateString(text));
                result->Finish(result->CreateVector(offsets));
                break;
            }
            case 13: {
                if (!ValidId(ctx, id, 2)) return 3;
                auto code = ctx.core.GetMethodOpCodes(id);
                return Copy(code.data(), code.size(), out);
            }
            case 14: {
                if (!ValidId(ctx, id, 1)) return 3;
                auto class_data = ctx.core.GetClassByIds({static_cast<int64_t>(id)});
                auto holder = flatbuffers::GetRoot<ClassMetaArrayHolder>(class_data->GetBufferPointer());
                if (!holder->classes() || holder->classes()->size() != 1) return 4;
                auto type = holder->classes()->Get(0);
                std::vector<int64_t> ids;
                if (type->methods()) for (auto method_id : *type->methods())
                    ids.push_back((int64_t(type->dex_id()) << 32) | uint32_t(method_id));
                result = ctx.core.GetMethodByIds(ids);
                break;
            }
            default: return 3;
        }
        if (!result) return 4;
        if (result->GetSize() > kMaxResult) return 5;
        auto data = result->GetBufferPointer();
        return Copy(data, result->GetSize(), out);
    } catch (...) { FatalException(); }
}

extern "C" int dk_smali(void *opaque, uint64_t id, uint8_t method, uint8_t strict,
    uint32_t max_output, DkBuffer *out, DkStatus *status) {
    if (!out || !status) return 1;
    *out = {}; *status = {};
    if (!opaque || method > 1 || strict > 1 || !max_output || max_output > kMaxResult) return 1;
    auto &ctx = *static_cast<Context *>(opaque);
    if (!ValidId(ctx, id, method ? 2 : 1)) return 3;
    try {
        dexkit::SmaliOptions options;
        options.debug = strict ? dexkit::SmaliDebugMode::Strict : dexkit::SmaliDebugMode::None;
        options.max_output_bytes = max_output;
        std::string text;
        auto s = method ? ctx.core.GetMethodSmali(id, options, text) : ctx.core.GetClassSmali(id, options, text);
        *status = {s.dex_offset, s.code_offset, s.detail, s.dex_id, s.member_id,
            uint8_t(s.error), uint8_t(s.phase), uint8_t(s.member_kind)};
        if (!s.ok()) return 6;
        return Copy(reinterpret_cast<const uint8_t *>(text.data()), text.size(), out);
    } catch (...) { FatalException(); }
}
