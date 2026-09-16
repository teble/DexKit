#include "string_admission_queries.h"
#include "benchmark_diagnostics.h"
#include "dex_item.h"
#include <array>

namespace {
using namespace string_fixture;
void Require(bool value) { if (!value) std::abort(); }

std::unique_ptr<Builder> CrossQuery(int variant) {
    auto b = std::make_unique<Builder>();
    const auto root_strings = Strings(*b, {{"OnlyOne"}});
    const auto second = Strings(*b, {{"Second"}});
    const auto remote_name = schema::CreateStringMatcher(*b, b->CreateString("remote"), schema::StringMatchType::Equal);
    schema::MethodMatcherBuilder child(*b);
    // The negative case deliberately reuses the exact root vector address.
    child.add_using_strings(variant == 2 ? second : root_strings);
    if (variant == 3) child.add_method_name(remote_name);
    const auto children = b->CreateVector(std::vector{child.Finish()});
    const auto invokes = schema::CreateMethodsMatcher(*b, children);
    schema::MethodMatcherBuilder root(*b);
    root.add_using_strings(root_strings);
    if (variant) root.add_invoking_methods(invokes);
    b->Finish(schema::CreateFindMethod(*b, 0, 0, false, 0, 0, false, root.Finish()));
    return b;
}
}

// This standalone checker is linked against the frozen artifacts, like the
// bitmap budget checker. The diagnostic friend entry needs no engine change.
void dexkit::BenchmarkDiagnostics::CheckStringAdmission(std::string_view apk) {
    std::array<std::string, 4> expected;
    for (int state = 0; state < 3; ++state) {
        for (int variant = 0; variant < 4; ++variant) {
            DexKit bridge(apk, 1); bridge.SetThreadNum(4);
            if (state == 1) bridge.InitFullCache();
            if (state == 2) {
                auto warm = string_admission_fixture::WarmupQuery();
                auto ignored = bridge.FindMethod(flatbuffers::GetRoot<schema::FindMethod>(warm->GetBufferPointer()));
            }
            auto data = CrossQuery(variant);
            const auto *query = flatbuffers::GetRoot<schema::FindMethod>(data->GetBufferPointer());
#if DEXKIT_EXPERIMENT_INVERTED_STRINGS
            for (const auto &dex : bridge.dex_items)
                Require(dex->inverted_strings_ready.load(std::memory_order_acquire) == (state == 2));
            if (variant) {
                const auto local = bridge.dex_items[0]->PlanRootStringCandidates(query->matcher());
                const auto remote = bridge.dex_items[1]->PlanRootStringCandidates(query->matcher());
                if (state == 2 && DEXKIT_EXPERIMENT_INVERTED_STRING_RANGES) {
                    Require(local.route == inverted_string::QueryPlan::Route::Range && local.postings == 1);
                    Require(remote.route == inverted_string::QueryPlan::Route::Empty && remote.postings == 0);
                } else {
                    Require(local.route == inverted_string::QueryPlan::Route::Legacy);
                    Require(remote.route == inverted_string::QueryPlan::Route::Legacy);
                }
            }
#endif
            auto result = bridge.FindMethod(query);
            const auto *methods = flatbuffers::GetRoot<schema::MethodMetaArrayHolder>(result->GetBufferPointer())->methods();
            Require(methods->size() == (variant == 0 || variant == 2 ? 1U : 0U));
            if (methods->size()) {
                const auto *method = methods->Get(0);
                Require(method->dex_id() == 0 && method->id() == 1);
                Require(method->dex_descriptor()->string_view() == "Ladmit/Caller;->call()V");
            }
            if (variant) {
                bool resolved = false;
                for (const auto &binding : bridge.dex_items[0]->method_cross_info)
                    resolved |= binding && binding.value().first == 1 && binding.value().second == 1;
                Require(resolved);
            }
            const std::string bytes(reinterpret_cast<const char *>(result->GetBufferPointer()), result->GetSize());
            if (state) Require(bytes == expected[variant]);
            else {
                expected[variant] = bytes;
                const auto size = static_cast<uint32_t>(bytes.size());
                Require(std::fwrite(&size, sizeof(size), 1, stdout) == 1);
                Require(std::fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size());
            }
        }
    }
    std::fprintf(stderr, "CHECK_STRING_ADMISSION_CROSS_DEX {\"queries\":4,\"states\":3,\"same_local_id\":1,\"passed\":true}\n");
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    dexkit::BenchmarkDiagnostics::CheckStringAdmission(argv[1]);
}
