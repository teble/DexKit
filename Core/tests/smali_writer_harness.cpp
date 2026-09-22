// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "../dexkit/smali/writer.h"
#include "../dexkit/smali/selection.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif

namespace {
using namespace dexkit;
using namespace dexkit::smali;
size_t calls = 0, rejected = 0;
void Require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << " after " << calls << " calls\n"; std::abort(); }
}

SmaliStatus Run(const std::vector<uint8_t>& bytes, uint32_t method, bool whole, bool strict,
                size_t output_limit = 1024 * 1024, size_t* emitted_size = nullptr) {
    SmaliOptions options;
    options.debug = strict ? SmaliDebugMode::Strict : SmaliDebugMode::None;
    options.max_input_bytes = 1024 * 1024;
    options.max_output_bytes = output_limit;
    options.max_items = 32768;
    options.max_code_units = 8192;
    std::string output = "unchanged";
    auto status = whole ? WriteClass(bytes, 0, 0, options, output)
                        : WriteMethod(bytes, 0, 0, method, options, output);
    ++calls;
    if (!status.ok()) {
        ++rejected;
        Require(output == "unchanged", "Failed output was not transactional");
    } else {
        Require(output != "unchanged" && !output.empty(), "Success without output");
        if (emitted_size) *emitted_size = output.size();
    }
    return status;
}

void Fixture(const char* path) {
    std::ifstream file(path, std::ios::binary);
    Require(bool(file), "Cannot open fixture; run SmaliOutputTest first");
    std::vector<uint8_t> original{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    SmaliOptions options;
    State state{options};
    CheckedDex dex(original, 0, state);
    dex::ClassDef owner;
    ClassMembers members;
    Require(dex.Init() && dex.ClassDef(0, owner) && SelectClass(dex, owner, members), "Invalid seed");
    Require(!members.direct_methods.empty() || !members.virtual_methods.empty(), "Seed has no methods");
    uint32_t method = !members.direct_methods.empty() ? members.direct_methods.front().index
                                                     : members.virtual_methods.front().index;
    for (bool strict : {false, true}) {
        Require(Run(original, method, true, strict).ok(), "Valid class seed failed");
        for (auto member : members.direct_methods)
            Require(Run(original, member.index, false, strict).ok(), "Valid direct method failed");
        for (auto member : members.virtual_methods)
            Require(Run(original, member.index, false, strict).ok(), "Valid virtual method failed");
    }
    // Byte perturbations visit headers, definition records, instructions,
    // payloads, handlers and metadata without entering the old DEX loader.
    for (size_t offset = 0; offset < original.size(); ++offset) {
        for (uint8_t mask : {uint8_t(1), uint8_t(0x80), uint8_t(0xff)}) {
            auto changed = original;
            changed[offset] ^= mask;
            Run(changed, method, true, offset & 1);
            Run(changed, method, false, !(offset & 1));
        }
    }
    for (size_t size = 0; size < original.size(); ++size) {
        std::vector<uint8_t> truncated(original.begin(), original.begin() + size);
        Run(truncated, method, true, size & 1);
    }
    // Force a late emission failure after the full class and its temporary
    // structures were visited, instead of testing only empty-image failures.
    size_t full_size = 0;
    Require(Run(original, method, true, true, 1024 * 1024, &full_size).ok(), "Cannot size output");
    Require(full_size > 2, "Empty class text");
    Require(Run(original, method, true, true, full_size - 2).error == SmaliError::LimitExceeded,
            "Missing late output limit failure");
    Member debug_method{};
    for (auto* group : {&members.direct_methods, &members.virtual_methods}) {
        for (auto member : *group) {
            uint16_t registers = 0;
            if (member.code_offset) std::memcpy(&registers, original.data() + member.code_offset, 2);
            if (registers) { debug_method = member; break; }
        }
        if (debug_method.code_offset) break;
    }
    Require(debug_method.code_offset != 0, "Seed needs a method with a register for deep debug checks");
    dex::MethodId declaration;
    std::string signature;
    std::vector<uint16_t> parameters;
    Require(dex.Entry(dex.header().method_ids_off, dex.header().method_ids_size, debug_method.index, declaration) &&
            dex.Proto(declaration.proto_idx, signature, &parameters), "Cannot read seed prototype");
    Require(parameters.size() < 128, "Seed parameter count exceeds this harness fixture encoder");
    std::vector<uint8_t> stream{1, uint8_t(parameters.size())};
    stream.insert(stream.end(), parameters.size(), 0); // Unknown parameter names.
    // A real line event is allocated before START_LOCAL_EXTENDED reaches an
    // incomplete signature ULEB at the physical end of an otherwise valid DEX.
    stream.insert(stream.end(), {14, 4, 0, 0, 0, 0x80});
    auto deep = original;
    auto header = dex.header();
    uint32_t extra = (uint32_t(stream.size()) + 3) & ~3u;
    deep.resize(deep.size() + extra);
    header.file_size += extra;
    header.data_size += extra;
    std::memcpy(deep.data(), &header, header.header_size);
    uint32_t debug_offset = deep.size() - stream.size();
    std::memcpy(deep.data() + debug_method.code_offset + 8, &debug_offset, 4);
    std::copy(stream.begin(), stream.end(), deep.begin() + debug_offset);
    auto truncated = Run(deep, debug_method.index, false, true);
    Require(truncated.error == SmaliError::MalformedInput && truncated.dex_offset == deep.size(),
            "Deep signature truncation did not reach the missing byte");
    // Warm allocator/runtime bookkeeping before checking retained live bytes.
    for (unsigned i = 0; i < 100; ++i) Run(original, method, true, i & 1);
#if defined(__APPLE__) && !__has_feature(address_sanitizer)
    malloc_statistics_t before{}, after{};
    malloc_zone_statistics(nullptr, &before);
#endif
    for (unsigned i = 0; i < 10000; ++i) {
        Require(Run(original, method, true, i & 1).ok(), "Repeated request failed");
        if (i & 1) {
            Require(Run(original, method, true, true, full_size - 2).error == SmaliError::LimitExceeded,
                    "Late failed request unexpectedly succeeded");
        } else {
            Require(Run(deep, debug_method.index, false, true).error == SmaliError::MalformedInput,
                    "Deep failed request unexpectedly succeeded");
        }
    }
#if defined(__APPLE__) && !__has_feature(address_sanitizer)
    malloc_zone_statistics(nullptr, &after);
    // Allocator metadata can vary; linear per-request growth must not survive.
    Require(after.size_in_use <= before.size_in_use + 65536, "Live native allocations kept growing");
    std::cout << "live bytes before=" << before.size_in_use << " after=" << after.size_in_use << '\n';
#endif
    std::cout << path << ": " << calls << " calls, " << rejected << " controlled rejections\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: dexkit_smali_writer_harness fixture.dex ...\n"; return 2; }
    for (int i = 1; i < argc; ++i) Fixture(argv[i]);
}
