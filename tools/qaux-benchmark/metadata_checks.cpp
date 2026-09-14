#include "benchmark_diagnostics.h"
#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: dexkit_metadata_checks demo.apk\n");
        return 2;
    }
    dexkit::BenchmarkDiagnostics::CheckMetadata(argv[1]);
}
