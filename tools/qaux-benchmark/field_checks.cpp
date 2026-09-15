#include "field_queries.h"
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>

namespace {
using namespace field_fixture;
void Require(bool condition) { if (!condition) std::abort(); }
template<typename Values>
void Describe(bool classes, int variant, const Values *values) {
    std::fprintf(stderr, "FIELD_RESULTS {\"classes\":%s,\"variant\":%d,\"results\":[", classes ? "true" : "false", variant);
    bool first = true;
    for (auto value : *values) {
        auto text = value->dex_descriptor()->string_view();
        std::fprintf(stderr, "%s[%u,%u,\"%.*s\"]", first ? "" : ",", value->dex_id(), uint32_t(value->id()), int(text.size()), text.data());
        first = false;
    }
    std::fprintf(stderr, "]}\n");
}
std::string Collect(DexKit &bridge, bool describe = false) {
    std::string out;
    for (bool classes : {false, true}) for (int variant = 0; variant < QueryCount; ++variant) {
        auto query = Query(classes, variant);
        auto data = classes ? bridge.FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()))
                            : bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
        Require(data != nullptr && data->GetSize() <= UINT32_MAX);
        if (describe) {
            if (classes) Describe(true, variant, flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(data->GetBufferPointer())->classes());
            else Describe(false, variant, flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods());
        }
        uint32_t size = data->GetSize();
        for (int i = 0; i < 4; ++i) out.push_back(char(size >> (i * 8)));
        out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), size);
    }
    // Check the public forward getter too: matching alone cannot prove that
    // every duplicate occurrence and access direction survives compaction.
    auto all = Query(false, 0);
    auto methods = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(all->GetBufferPointer()));
    for (auto method : *flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(methods->GetBufferPointer())->methods()) {
        const int64_t id = (int64_t(method->dex_id()) << 32) | uint32_t(method->id());
        auto data = bridge.GetUsingFields(id);
        Require(data != nullptr && data->GetSize() <= UINT32_MAX);
        uint32_t size = data->GetSize();
        for (int i = 0; i < 4; ++i) out.push_back(char(size >> (i * 8)));
        out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), size);
        if (describe) {
            std::fprintf(stderr, "FIELD_USES {\"dex\":%u,\"method\":%u,\"uses\":[", method->dex_id(), uint32_t(method->id()));
            bool first = true;
            for (auto use : *flatbuffers::GetRoot<schema::UsingFieldMetaArrayHolder>(data->GetBufferPointer())->items()) {
                auto field = use->field();
                auto text = field->dex_descriptor()->string_view();
                std::fprintf(stderr, "%s[%u,%u,\"%.*s\",%s]", first ? "" : ",", field->dex_id(), uint32_t(field->id()),
                    int(text.size()), text.data(), use->using_type() == schema::UsingType::Get ? "true" : "false");
                first = false;
            }
            std::fprintf(stderr, "]}\n");
        }
    }
    return out;
}
}
int main(int argc, char **argv) {
    const bool dump = argc == 3 && std::string_view(argv[1]) == "--dump";
    if (argc != 2 && !dump) return 2;
    DexKit reference(argv[argc - 1], 1);
    reference.SetThreadNum(4); reference.InitFullCache();
    const auto expected = Collect(reference, true);
    for (int sequence = 0; sequence < 3; ++sequence) {
        DexKit bridge(argv[argc - 1], 1); bridge.SetThreadNum(4);
        if (sequence == 1) bridge.InitFullCache();
        if (sequence == 2) {
            std::latch start(1);
            std::thread a([&] { start.wait(); Require(Collect(bridge) == expected); });
            std::thread b([&] { start.wait(); Require(Collect(bridge) == expected); });
            std::thread c([&] { start.wait(); bridge.InitFullCache(); });
            start.count_down(); a.join(); b.join(); c.join();
        }
        Require(Collect(bridge) == expected); Require(Collect(bridge) == expected);
        bridge.InitFullCache(); Require(Collect(bridge) == expected);
    }
    if (dump) Require(std::fwrite(expected.data(), 1, expected.size(), stdout) == expected.size());
    else std::printf("CHECK_FIELDS {\"queries\":%d,\"sequences\":3,\"passed\":true}\n", QueryCount * 2);
}
