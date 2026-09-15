#include "benchmark_diagnostics.h"
#include <cstdio>
#include <string_view>

int main(int argc, char **argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--dense") {
        dexkit::BenchmarkDiagnostics::CheckDenseDescriptors(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--dump") {
        dexkit::BenchmarkDiagnostics::DumpSymbols(argv[2]);
        return 0;
    }
    if (argc != 2) {
        std::fprintf(stderr, "Usage: dexkit_symbol_checks [--dump] symbols.apk\n");
        return 2;
    }
    dexkit::BenchmarkDiagnostics::CheckSymbols(argv[1]);
}
