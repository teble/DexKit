#include "benchmark_diagnostics.h"
#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: dexkit_symbol_checks symbols.apk\n");
        return 2;
    }
    dexkit::BenchmarkDiagnostics::CheckSymbols(argv[1]);
}
