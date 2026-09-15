// Reuse the exact measured query builder; assertions stay outside timed code.
#define main StringWorkloadMain
#include "string_workload.cpp"
#undef main

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    string_fixture::DexKit bridge(argv[1], 1);
    bridge.SetThreadNum(4);
    auto positive = NestedBroadQuery(false), negative = NestedBroadQuery(true);
    for (unsigned pass = 0; pass < 16; ++pass) {
        for (bool absent : {false, true}) {
            const auto &query = absent ? negative : positive;
            auto result = bridge.FindMethod(flatbuffers::GetRoot<dexkit::schema::FindMethod>(query->GetBufferPointer()));
            const auto *methods = flatbuffers::GetRoot<dexkit::schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods();
            Require(methods->size() == (absent ? 0U : 4500U));
            for (uint32_t id = 0; id < methods->size(); ++id) {
                char expected[64];
                std::snprintf(expected, sizeof(expected), "Lsinv/Broad;->m%05u()V", id);
                Require(methods->Get(id)->dex_id() == 0 && methods->Get(id)->id() == id);
                Require(methods->Get(id)->dex_descriptor()->string_view() == expected);
            }
        }
    }
    std::printf("{\"passes\":16,\"positive_per_pass\":4500,\"negative_per_pass\":0,\"complete_ordered_ids_and_descriptors\":true}\n");
}
