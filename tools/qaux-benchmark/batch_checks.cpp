#include "batch_queries.h"
#include <cstdio>
#include <latch>
#include <thread>

namespace {
using namespace batch_fixture;
void Require(bool value) { if (!value) std::abort(); }

template<typename Values>
void Members(const Values *values) {
    bool first = true;
    for (auto value : *values) {
        auto name = value->dex_descriptor()->string_view();
        std::fprintf(stderr, "%s[%u,%u,\"%.*s\"]", first ? "" : ",", value->dex_id(),
                     uint32_t(value->id()), int(name.size()), name.data());
        first = false;
    }
}

void Describe(bool classes, int variant, const Builder &data) {
    std::fprintf(stderr, "BATCH_RESULTS {\"classes\":%s,\"variant\":%d,\"groups\":[", classes ? "true" : "false", variant);
    bool first = true;
    const auto key = [&](auto item) {
        const auto value = item->union_key()->string_view();
        std::fprintf(stderr, "%s[\"%.*s\",[", first ? "" : ",", int(value.size()), value.data());
        first = false;
    };
    if (classes) {
        for (auto item : *flatbuffers::GetRoot<schema::BatchClassMetaArrayHolder>(data.GetBufferPointer())->items()) {
            key(item); Members(item->classes()); std::fprintf(stderr, "]]");
        }
    } else {
        for (auto item : *flatbuffers::GetRoot<schema::BatchMethodMetaArrayHolder>(data.GetBufferPointer())->items()) {
            key(item); Members(item->methods()); std::fprintf(stderr, "]]");
        }
    }
    std::fprintf(stderr, "]}\n");
}

std::string Collect(DexKit &bridge, bool describe = false) {
    std::string out;
    for (bool classes : {false, true}) for (int variant = 0; variant < QueryCount; ++variant) {
        auto query = Query(classes, variant);
        auto data = Run(bridge, *query, classes);
        Require(data != nullptr && data->GetSize() <= UINT32_MAX);
        if (describe) Describe(classes, variant, *data);
        uint32_t size = data->GetSize();
        for (int i = 0; i < 4; ++i) out.push_back(char(size >> (i * 8)));
        out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), size);
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
    for (bool warm : {false, true}) {
        DexKit bridge(argv[argc - 1], 1); bridge.SetThreadNum(4);
        if (warm) bridge.InitFullCache();
        Require(Collect(bridge) == expected);
        Require(Collect(bridge) == expected);
        std::latch start(1);
        std::thread a([&] { start.wait(); Require(Collect(bridge) == expected); });
        std::thread b([&] { start.wait(); Require(Collect(bridge) == expected); });
        start.count_down(); a.join(); b.join();
        bridge.InitFullCache(); Require(Collect(bridge) == expected);
    }
    if (dump) Require(std::fwrite(expected.data(), 1, expected.size(), stdout) == expected.size());
    else std::printf("CHECK_BATCH {\"queries\":%d,\"passed\":true}\n", QueryCount * 2);
}
