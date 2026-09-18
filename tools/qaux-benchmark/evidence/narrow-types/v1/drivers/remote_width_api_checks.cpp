#include "dexkit.h"
#include <cstdio>
#include <cstdlib>
using namespace dexkit;
void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "Remote width check failed at line %d\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    DexKit bridge(argv[1], 1);
    bridge.SetThreadNum(4);
    for (int stage = 0; stage < 2; ++stage) {
        if (stage) bridge.InitFullCache();
        auto data = bridge.GetInvokeMethods(1);
        const auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods();
        Require(methods->size() == 1 && methods->Get(0)->dex_id() == 1 && methods->Get(0)->id() == 65536);
        for (int64_t id : {int64_t{0}, (int64_t{1} << 32) | 65536}) {
            auto target = bridge.GetMethodByIds({id});
            const auto targets = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(target->GetBufferPointer())->methods();
            Require(targets->size() == 1);
            const auto meta = targets->Get(0);
            Require(meta->dex_id() == 1 && meta->id() == 65536);
            auto callers = bridge.GetCallMethods(id);
            const auto rows = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(callers->GetBufferPointer())->methods();
            // The existing caller getter reads the physical row: aggregation
            // clears the alias row and retains edges on the definition row.
            if (id == 0) Require(rows->size() == 0);
            else Require(rows->size() == 1 && rows->Get(0)->dex_id() == 0 && rows->Get(0)->id() == 1);
        }
        auto data_class = bridge.GetClassData("L0External;");
        const auto meta_class = flatbuffers::GetRoot<schema::ClassMeta>(data_class->GetBufferPointer());
        Require(meta_class->methods()->size() == 1 && meta_class->methods()->Get(0) == 65536);
    }
    std::puts("CHECK_REMOTE_WIDTH {\"local_operand\":0,\"remote_method\":65536,\"forward\":true,\"reverse\":true,\"public_ids\":true}");
}
