#include "invocation_queries.h"
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>

namespace {
using namespace invocation_fixture;
void Require(bool condition) { if (!condition) std::abort(); }
std::string Collect(DexKit &bridge, bool callers) {
    std::string out;
    for (int variant = 0; variant < 10; ++variant) {
        auto query = Query(callers, variant);
        auto data = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(query->GetBufferPointer()));
        Require(data != nullptr);
        auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods();
        if (variant == 0) Require(methods->size() != 0);
        if (variant == 1 || variant == 4) Require(methods->size() == 0);
        uint32_t length = data->GetSize();
        for (size_t i = 0; i < 4; ++i) out.push_back(char(length >> (8 * i)));
        out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), length);
    }
    return out;
}
}

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && std::string_view(argv[1]) == "--dump")) return 2;
    const auto apk = argv[argc - 1];
    DexKit reference(apk, 1);
    reference.SetThreadNum(4);
    reference.InitFullCache();
    const auto forward = Collect(reference, false), reverse = Collect(reference, true);
    for (int sequence = 0; sequence < 3; ++sequence) {
        DexKit bridge(apk, 1);
        bridge.SetThreadNum(4);
        if (sequence == 1) Require(Collect(bridge, true) == reverse);
        if (sequence == 2) {
            std::latch start(1);
            std::thread a([&] { start.wait(); Require(Collect(bridge, false) == forward); });
            std::thread b([&] { start.wait(); Require(Collect(bridge, true) == reverse); });
            start.count_down(); a.join(); b.join();
        }
        Require(Collect(bridge, false) == forward);
        Require(Collect(bridge, true) == reverse);
        bridge.InitFullCache();
        Require(Collect(bridge, false) == forward);
        Require(Collect(bridge, true) == reverse);
    }
    if (argc == 3) {
        for (auto *bytes : {&forward, &reverse})
            Require(std::fwrite(bytes->data(), 1, bytes->size(), stdout) == bytes->size());
    } else std::puts("CHECK_INVOCATIONS {\"cases\":20,\"sequences\":3,\"passed\":true}");
}
