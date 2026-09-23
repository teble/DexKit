// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "dexkit.h"
#include "schema/querys_generated.h"
#include "schema/results_generated.h"

// Test-only independent C++ producer. Valid smali-assembled fixture inputs.
extern "C" int dexkit_reference_matches(const uint8_t* dex, size_t dex_size,
                                       const uint8_t* text, size_t text_size) {
    try {
        dexkit::DexKit core;
        core.SetThreadNum(2);
        if (core.AddDex(const_cast<uint8_t*>(dex),dex_size) != dexkit::Error::SUCCESS) return -1;
        using namespace dexkit::schema;
        flatbuffers::FlatBufferBuilder b;
        auto value=b.CreateString(reinterpret_cast<const char*>(text),text_size);
        auto str=CreateStringMatcher(b,value,StringMatchType::Equal);
        auto strings=b.CreateVector(std::vector{str});
        auto name=CreateStringMatcher(b,b.CreateString("strings"),StringMatchType::Equal);
        MethodMatcherBuilder matcher(b);
        matcher.add_method_name(name); matcher.add_using_strings(strings);
        auto m=matcher.Finish();
        auto query=CreateFindMethod(b,0,0,false,0,0,false,m);
        b.Finish(query);
        auto result=core.FindMethod(flatbuffers::GetRoot<FindMethod>(b.GetBufferPointer()));
        return flatbuffers::GetRoot<MethodMetaArrayHolder>(result->GetBufferPointer())->methods()->size();
    } catch (...) { return -1; }
}
