#include "dexkit.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace dexkit;
void Require(bool value, const char *label) {
    if (!value) { std::fprintf(stderr, "Width API check failed: %s\n", label); std::abort(); }
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const bool wide = std::string(argv[2]) == "wide";
    const uint32_t last = wide ? 65536 : 65535;
    DexKit bridge(argv[1], 1);
    bridge.SetThreadNum(4);
    bridge.InitFullCache();
    auto source_data = bridge.GetMethodData(wide ? "Lwidth/Source;->m65535()V" : "Lwidth/Source;->m65534()V");
    Require(source_data != nullptr, "source found");
    const auto source = flatbuffers::GetRoot<schema::MethodMeta>(source_data->GetBufferPointer());
    Require(source->dex_id() == 0 && source->id() == last, "source ID boundary");

    auto class_data = bridge.GetClassData("Lwidth/Source;");
    const auto klass = flatbuffers::GetRoot<schema::ClassMeta>(class_data->GetBufferPointer());
    Require(klass->methods()->size() == last, "public class member count");
    Require(klass->methods()->Get(0) == 1 && klass->methods()->Get(last-1) == last, "public widened IDs");
    Require(bridge.GetMethodOpCodes(last).size() == 70004, "opcode length above 65535");
    const auto strings = bridge.GetUsingStrings(last);
    Require(strings.size() == 1 && strings[0] == "zz-jumbo-width-needle", "jumbo string ID");

    const auto invoked_data = bridge.GetInvokeMethods(last);
    const auto invoked = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(invoked_data->GetBufferPointer())->methods();
    Require(invoked->size() == 70000, "long invoke length");
    for (uint32_t i = 0; i < invoked->size(); ++i) {
        const auto item = invoked->Get(i);
        const auto which = i % (wide ? 2 : 3);
        Require(item->dex_id() == (which == 0 ? 1U : 0U), "invoke DEX order");
        Require(item->id() == (which == 0 ? 0U : which == 1 ? 1U : last), "invoke ID order/duplicates");
    }
    auto caller_data = bridge.GetCallMethods(int64_t{1} << 32);
    const auto callers = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(caller_data->GetBufferPointer())->methods();
    Require(callers->size() == (wide ? 35000U : 23334U), "cross-DEX caller count");
    for (const auto item : *callers) Require(item->dex_id() == 0 && item->id() == last, "high caller IDs");

    const auto check_field = [&](auto data) {
        const auto methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(data->GetBufferPointer())->methods();
        Require(methods->size() == 1 && methods->Get(0)->dex_id() == 0 && methods->Get(0)->id() == last,
                "field reverse high method ID");
    };
    check_field(bridge.FieldGetMethods(0));
    check_field(bridge.FieldPutMethods(0));
    std::printf("CHECK_WIDTH_API {\"wide\":%s,\"last_id\":%u,\"opcode_count\":70004,\"invokes\":70000,"
                "\"callers\":%u,\"jumbo\":true,\"field_reverse\":true,\"public_ids\":true}\n",
                wide ? "true" : "false", last, callers->size());
}
