#include "source_queries.h"
#include "benchmark_diagnostics.h"
#include "dex_item.h"
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <map>
#include <thread>

namespace {
using namespace source_fixture;
void Require(bool condition) { if (!condition) std::abort(); }
void Append(std::string &out, std::unique_ptr<Builder> data) {
    Require(data != nullptr && data->GetSize() <= UINT32_MAX);
    uint32_t length = data->GetSize();
    for (size_t i = 0; i < 4; ++i) out.push_back(char(length >> (8 * i)));
    out.append(reinterpret_cast<const char *>(data->GetBufferPointer()), length);
}
std::string Collect(DexKit &bridge, const std::vector<int64_t> &ids) {
    std::string out;
    for (int variant = 0; variant < 16; ++variant) {
        auto query = Query(variant);
        auto data = bridge.FindClass(flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer()));
        Require(data != nullptr);
        auto classes = flatbuffers::GetRoot<schema::ClassMetaArrayHolder>(data->GetBufferPointer())->classes();
        if (variant == 1 || variant == 10 || variant == 11 || variant == 12) Require(classes->size() != 0);
        if (variant == 2 || variant == 6) Require(classes->size() == 0);
        Append(out, std::move(data));
    }
    Append(out, bridge.GetClassByIds(ids));
    Append(out, bridge.GetClassData("Lsources/Shared;"));
    return out;
}
std::string Expected(std::string_view name, uint32_t dex) {
    if (name == "Lsources/Shared;") return dex == 0 ? "First.java" : "Last.java";
    static const std::map<std::string_view, std::string> known = {
        {"Lsources/Case;", "MiXeD.java"}, {"Lsources/Utf;", UtfSource},
        {"Lsources/Mutf;", MutfSource}, {"Lsources/Long;", LongSource()},
        {"Lsources/Ref;", "Rare.java"}
    };
    if (auto found = known.find(name); found != known.end()) return found->second;
    if (name.starts_with("Lsources/Bulk")) {
        const auto start = name.find('_') + 1;
        const auto index = std::strtoul(name.data() + start, nullptr, 10);
        if (index == 0) return "Rare.java";
        return std::array<std::string, 4>{"", "", "Shared.java", "MiXeD.java"}[index % 4];
    }
    return {};
}
}

void dexkit::BenchmarkDiagnostics::CheckSources(std::string_view apk, bool dump) {
    DexKit reference(apk, 1);
    reference.SetThreadNum(4);
    reference.InitFullCache();
    std::vector<int64_t> ids;
    std::array<bool, 2> checked_indexes{};
    for (const auto &item : reference.dex_items) {
        for (size_t i = 0; i < item->reader.TypeIds().size(); ++i) {
            const auto name = item->type_names[i];
            if (item->type_def_flag[i] && (name == "Lsources/Absent;" || name == "Lsources/Empty;")) {
                const auto source = item->reader.ClassDefs()[item->type_def_idx[i]].source_file_idx;
                if (name == "Lsources/Absent;") { Require(source == dex::kNoIndex); checked_indexes[0] = true; }
                else { Require(source != dex::kNoIndex && item->strings[source].empty()); checked_indexes[1] = true; }
            }
            auto bean = item->GetClassBean(i);
            // A defined Shared in DEX 0 stays local; undefined references use
            // the final declared Shared. Missing and empty both serialize empty.
            Require(bean.source_file == Expected(name, item->dex_id));
            ids.push_back((int64_t(item->dex_id) << 32) | i);
            if (!item->type_def_flag[i]) {
                auto query = Query(1);
                auto matcher = flatbuffers::GetRoot<schema::FindClass>(query->GetBufferPointer())->matcher();
                Require(!item->IsClassSmaliSourceMatched(i, matcher->smali_source()));
            }
        }
    }
    Require(checked_indexes[0] && checked_indexes[1]);
    const auto expected = Collect(reference, ids);
    for (int sequence = 0; sequence < 3; ++sequence) {
        DexKit bridge(apk, 1);
        bridge.SetThreadNum(4);
        if (sequence == 1) bridge.InitFullCache();
        if (sequence == 2) {
            std::latch start(1);
            std::thread a([&] { start.wait(); Require(Collect(bridge, ids) == expected); });
            std::thread b([&] { start.wait(); Require(Collect(bridge, ids) == expected); });
            start.count_down(); a.join(); b.join();
        }
        Require(Collect(bridge, ids) == expected);
        bridge.InitFullCache();
        Require(Collect(bridge, ids) == expected);
    }
    if (dump) Require(std::fwrite(expected.data(), 1, expected.size(), stdout) == expected.size());
    else std::printf("CHECK_SOURCES {\"queries\":16,\"type_ids\":%zu,\"sequences\":3,\"passed\":true}\n", ids.size());
}

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && std::string_view(argv[1]) == "--dump")) return 2;
    dexkit::BenchmarkDiagnostics::CheckSources(argv[argc - 1], argc == 3);
}
