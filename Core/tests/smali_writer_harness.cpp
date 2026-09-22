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

SmaliStatus Run(const std::vector<uint8_t>& bytes, uint32_t method, bool whole, bool strict) {
    SmaliOptions options;
    options.debug = strict ? SmaliDebugMode::Strict : SmaliDebugMode::None;
    options.max_input_bytes = 1024 * 1024;
    options.max_output_bytes = 1024 * 1024;
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
    // Warm allocator/runtime bookkeeping before checking retained live bytes.
    for (unsigned i = 0; i < 100; ++i) Run(original, method, true, i & 1);
#if defined(__APPLE__) && !__has_feature(address_sanitizer)
    malloc_statistics_t before{}, after{};
    malloc_zone_statistics(nullptr, &before);
#endif
    for (unsigned i = 0; i < 10000; ++i) {
        Require(Run(original, method, true, i & 1).ok(), "Repeated request failed");
        std::vector<uint8_t> empty;
        Require(!Run(empty, method, false, false).ok(), "Empty image accepted");
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
