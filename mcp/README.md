# DexKit native MCP

Experimental native MCP server supporting Streamable HTTP and stdio, built in
Rust and statically linked to the
existing C++ Core. The server does not require a JVM at runtime. Java/Gradle and
the repository Android setup are still needed to assemble test fixtures and run
the existing JVM/Android regression suite.

## Build and run

Requires Rust 1.88+, CMake, a C++20 compiler and zlib development files. Native
linking is configured for macOS and Linux. The implementation is being validated
on macOS arm64; see VALIDATION.md for the exact executed checks and limitations.

```sh
cargo build --release --manifest-path mcp/Cargo.toml --locked
mcp/target/release/dexkit-mcp --transport http --listen 127.0.0.1:7331 --allow-root /path/to/apks
```

Connect an HTTP MCP client to `http://127.0.0.1:7331/mcp` while the process is
running. This is the official SDK Streamable HTTP endpoint, with request-scoped
SSE and stateless routing. Modern clients use per-request metadata; older clients
can initialize without receiving a transport session ID.

One native worker keeps business instances alive across HTTP requests and client
reconnections. All local clients share the configured roots, instances and
quotas. Use the returned `instanceId` in later calls and close it explicitly;
HTTP disconnect does not close a DEX instance. The server accepts only loopback
listen addresses and validates Host/Origin. This mode is for a trusted local
user, without remote authentication or tenant isolation.

HTTP bodies are limited to 2 MiB and ten seconds to read; up to 16 active HTTP
requests/response streams are allowed. The HTTP read deadline does not limit
Core execution time. SIGINT/SIGTERM shuts down HTTP and releases the worker.
The existing native queue and business request/result limits still apply.

For command-based MCP clients, stdio remains the default and is selectable with
`--transport stdio`. Example configuration (use absolute paths):

```json
{
  "mcpServers": {
    "dexkit": {
      "command": "/path/to/DexKit/mcp/target/release/dexkit-mcp",
      "args": ["--transport", "stdio", "--allow-root", "/path/to/apks"]
    }
  }
}
```

Repeat `--allow-root` for additional input directories. By default only the
current directory and its descendants are accepted. `--dump-schema` prints the
tool contracts. Diagnostics go to stderr; stdout stays reserved for stdio MCP.
File-polling and the deprecated separate HTTP+SSE endpoints are not implemented.

Core threads default to automatic CPU detection (`--threads 0`, with at least
one thread). Use `--threads 4`, for example, to override the count for every
instance. `capabilities.nativeThreads` reports the effective count. This applies
to parallel DEX loading, cache initialization and queries in both transports;
native MCP calls still execute one at a time. It is not a process-wide thread cap.

There is no default input-file size ceiling (`--max-input-mib 0`). The separate
`--max-dex-mib` limit defaults to 512 MiB of raw DEX bytes or total uncompressed
DEX bytes in an APK. Set it higher for larger codebases, or to `0` to disable it.
Both options apply to HTTP and stdio; capabilities reports their effective byte
values as `maxInputBytes` and `maxDexBytes`, where zero means unlimited.

The adapter validates the canonical path, regular-file type and input byte
budget, then passes the path to Core's APK/raw DEX loader through a thin C ABI.
Core owns the mappings. Raw DEX and aligned stored entries use file-backed data;
unaligned entries are copied for alignment and deflated entries are decompressed.
Open returns `instanceId`, `byteLength` and `dexCount` without hashing the input.
No full-APK copy, temporary APK or source-change snapshot is created.

Keep the source file and its path unchanged until the instance is closed. Close
it before modifying, replacing or deleting the input, then reopen if needed.
The server does not detect or recover from source changes. Allowed-root checks
apply to the canonical path at open; they are a local access policy, not a
security boundary against concurrent filesystem changes.

Core validates aggregate DEX budgets and entry headers before parallel
initialization. DEX byte limits do not bound indexes, query memory or execution
time. The adapter does not implement ZIP extraction or DEX initialization.
Duplicate-class declaration lookup consistently prefers the last logical DEX.

## Discover query parameters

Tool names use the stable `dexkit_` prefix. The program version is reported in
server information; `apiMajor` and `contractRevision` describe the business
contract through capabilities and query help. `schemaHash` identifies the input
schema only. These metadata fields do not automatically negotiate tool versions.

If a client displays `query: unknown` or hides nested matcher fields, call
`dexkit_get_query_schema` with the full `find_classes`, `find_methods` or
`find_fields` tool name. Help requires no open APK and remains available while
the native worker is busy or unavailable.

```json
{"tool":"dexkit_find_methods"}
```

Omitting `pointer` returns an overview, parameter links, rules and examples.
Passing `"pointer":""` returns the complete input schema. Other JSON Pointers
must refer to that same document; copy them from `links` to read exact fragments
or follow recursive types. Fragments have `standalone:false`; their references
resolve against the full document, not against the help reply. `ifSchemaHash`
optionally rejects a request based on an older document. Schema hashes cover
the schema only, not the native implementation or explanatory notes.

The contracts come from the same final schemas as `tools/list` and
`--dump-schema`. Small `select` enums are inlined; full matcher contracts and
runtime validation are preserved. This supplies missing information as tool
results; it does not disable client schema compaction or guarantee that a client
will render the original query signature in full. Each help reply is bounded to
16 KiB of business JSON and 64 KiB for the serialized MCP tool result (text plus
structured content, excluding JSON-RPC/SSE framing); oversized sections return
an error and child pointers rather than truncated JSON. `linksComplete:false`
marks an incomplete navigation list. The capability response
lists the MCP tool, while the underlying analysis library retains its own tools.

## Layout

```text
mcp/
  Cargo.toml, Cargo.lock
  crates/
    dexkit-sys/       native build and private C ABI
    dexkit-rs/        typed contract, Planus mapping and analysis state
    dexkit-mcp/       MCP SDK, HTTP/stdio and isolated native worker
  tests/             MCP HTTP/pipe and schema tests
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

An optional installed-Codex integration check uses a local scripted model
endpoint and makes no real model/API calls:

```sh
python3 mcp/tests/test_codex_discovery.py --codex /absolute/path/to/codex
```

It checks model-visible declarations, help delivery and scripted real queries.
It does not measure autonomous model tool selection or first-call success rates.

The first command assembles fixtures, runs the independent compatibility suite,
builds the MCP server, exercises real pipes and HTTP sockets plus the official
HTTP SDK client, and validates captured requests and
responses against the schemas actually returned by tools/list. Test-only schema
validation libraries do not enter the release executable.

See the [English guide](../doc-source/src/en/guide/mcp.md) or
[Chinese guide](../doc-source/src/zh-cn/guide/mcp.md) for query semantics and limits.

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
   Keep tool names stable for compatible changes. Only introduce distinct tool
   names or server entry points when incompatible contracts must coexist.

The current normal-Unicode contract rejects isolated UTF-16 surrogates on the
Rust side. Smali is escaped text and may still represent those DEX units without
putting an invalid Unicode value in JSON. The JVM codec preserves UTF-16 units
directly, including unpaired ones, and does not change FlatBuffers' global codec.
