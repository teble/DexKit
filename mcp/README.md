# DexKit native MCP

Experimental native stdio MCP server built in Rust and statically linked to the
existing C++ Core. The server does not require a JVM at runtime. Java/Gradle and
the repository Android setup are still needed to assemble test fixtures and run
the existing JVM/Android regression suite.

## Build and run

Requires Rust 1.88+, CMake, a C++20 compiler and zlib development files. Native
linking is configured for macOS and Linux. The implementation is being validated
on macOS arm64; see VALIDATION.md for the exact executed checks and limitations.

```sh
cargo build --release --manifest-path mcp/Cargo.toml --locked
mcp/target/release/dexkit-mcp --allow-root /path/to/apks
```

The process communicates through standard MCP pipes. Its stdout contains only
protocol messages; diagnostics go to stderr. It is not an interactive terminal
prompt. Repeat `--allow-root` for additional input directories. By default only
the current directory and its descendants are accepted.

Example MCP client configuration (use absolute paths on your machine):

```json
{
  "mcpServers": {
    "dexkit": {
      "command": "/path/to/DexKit/mcp/target/release/dexkit-mcp",
      "args": ["--allow-root", "/path/to/apks"]
    }
  }
}
```

`--dump-schema` prints the current tool contracts. HTTP and file-polling
transports are not implemented. The official rmcp SDK manages protocol versions,
including the legacy initialization flow and the 2026-07-28 metadata flow.

## Layout

```text
mcp/
  Cargo.toml, Cargo.lock
  crates/
    dexkit-sys/       native build and private C ABI
    dexkit-rs/        typed contract, Planus mapping and analysis state
    dexkit-mcp/       MCP SDK, stdio and isolated native worker
  tests/             MCP pipe and schema tests
  target/            ignored build outputs
tests/interop/planus/ independent Core compatibility tests, without MCP SDK
```

The source of native wire types stays in `schema/fbs`. Generated Rust bindings
are placed only in Cargo OUT_DIR. The generator, runtime and string adaptation
are pinned together. Android targets do not link Rust, the C ABI adapter, JSON
or MCP dependencies.

## Tests

From the repository root, after configuring Java/Android as for the main project:

```sh
python3 mcp/test.py
cargo clippy --manifest-path mcp/Cargo.toml --workspace --all-targets --locked -- -D warnings
cargo fmt --manifest-path mcp/Cargo.toml --all -- --check
```

The first command assembles fixtures, runs the independent compatibility suite,
builds the MCP server, exercises real pipes and validates captured requests and
responses against the schemas actually returned by tools/list. Test-only schema
validation libraries do not enter the release executable.

See the [English guide](../doc-source/src/en/guide/run-on-desktop.md) or
[Chinese guide](../doc-source/src/zh-cn/guide/run-on-desktop.md) for query semantics and limits.

## Extending the contract

1. Add a typed DTO field/variant or a separate tool for a different operation.
2. Implement validation and explicit native mapping. Keep old defaults stable.
3. Classify FBS fields/enums/unions in `crates/dexkit-rs/coverage.json`; classify
   every string as DEX MUTF-8, ordinary UTF-8 or ASCII. The build fails when
   schema additions/removals are not reflected in this manifest. This does not
   replace semantic tests or detect every change to the Core C++ API.
4. Add positive and negative real-Core cases proving the new condition changes
   results. Check existing requests still behave the same.
5. Update the tool registry, capability information, contract revision and docs.
   Schemars generates the tool schemas from DTOs; do not maintain another manual
   schema that can drift. Breaking types/defaults/semantics require a new API
   major, rather than silently interpreting old queries differently.

The current normal-Unicode contract rejects isolated UTF-16 surrogates on the
Rust side. Smali is escaped text and may still represent those DEX units without
putting an invalid Unicode value in JSON. The JVM codec preserves UTF-16 units
directly, including unpaired ones, and does not change FlatBuffers' global codec.
