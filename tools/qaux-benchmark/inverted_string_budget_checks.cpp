#include "batch_queries.h"
#include <cstdio>
#include <cstdlib>

using namespace batch_fixture;

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    DexKit bridge(argv[1], 1);
    bridge.SetThreadNum(4);
    std::vector<Atom> atoms;
    for (unsigned i = 0; i < 10000; ++i)
        atoms.push_back({"NeverInPool-" + std::to_string(i), T::Contains});
    auto query = Build(false, {{"budget", atoms}});
    std::string expected;
    for (int phase = 0; phase < 2; ++phase) {
        std::fprintf(stderr, "BUDGET_BEGIN phase=%d\n", phase);
        auto data = Run(bridge, *query, false);
        const auto *items = flatbuffers::GetRoot<schema::BatchMethodMetaArrayHolder>(data->GetBufferPointer())->items();
        if (items->size() != 1 || items->Get(0)->methods()->size() != 0) std::abort();
        std::string actual(reinterpret_cast<const char *>(data->GetBufferPointer()), data->GetSize());
        if (phase == 0) expected = actual;
        if (actual != expected) std::abort();
        std::fprintf(stderr, "BUDGET_END phase=%d\n", phase);
        // The first budget fallback must not construct the index. A following
        // small eligible query must still work and can construct it normally.
        auto positive = string_fixture::WorkQuery(false, "Needle", T::Equal);
        auto result = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(positive->GetBufferPointer()));
        const auto *methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods();
        if (methods->size() == 0) std::abort();
        bridge.InitFullCache();
    }
    if (std::fwrite(expected.data(), 1, expected.size(), stdout) != expected.size()) std::abort();
}
