# Native MCP server

The experimental `dexkit-mcp` executable uses Rust and the existing C++ Core.
It offers **Streamable HTTP** and **stdio** MCP transports without a JVM at
runtime. Native linking is configured for macOS and Linux; local validation is
on macOS arm64. Windows is not yet supported by this adapter.

Build from the repository root with Rust 1.88+, CMake, a C++20 compiler and zlib
development files:

```sh
cargo build --release --manifest-path mcp/Cargo.toml --locked
```

## Streamable HTTP

Start the server and keep it running:

```sh
mcp/target/release/dexkit-mcp --transport http --listen 127.0.0.1:7331 --allow-root /path/to/apks
```

Connect your MCP client to `http://127.0.0.1:7331/mcp`. For clients with a
JSON `mcpServers` URL configuration, the entry typically looks like this
(use your client's documented HTTP transport fields):

```json
{
  "mcpServers": {
    "dexkit": {"url": "http://127.0.0.1:7331/mcp"}
  }
}
```

The official MCP SDK handles JSON-RPC and request-scoped SSE responses at this
single endpoint. This is Streamable HTTP, not the deprecated separate SSE/message
endpoint transport. The 2026-07-28 protocol works without `initialize`; legacy
clients can still initialize and use `MCP-Protocol-Version`. HTTP routing is
stateless and does not issue `Mcp-Session-Id`; GET and DELETE return 405.
`resources/read` is an MCP POST operation, not an HTTP GET to an artifact URL.

**DEX state is independent of HTTP connections.** A single native worker keeps
opened inputs and their `instanceId` handles available across requests, new TCP
connections and repeated client initialization. Call `dexkit_close` to free
an instance; disconnecting a client does not close it. Server exit invalidates
all handles. This is a local single-user service: all connected clients share
allowed roots, analysis state and quotas. Client names and instance IDs do not
provide authentication or tenant isolation.

Only loopback listen addresses are accepted. Host and Origin are checked against
the actual bound address and localhost at the same port; native clients may omit
Origin. Browser Origins from other sites are rejected. This build provides no
remote deployment or authentication mode. Port 0 selects a free local port;
the actual endpoint is printed to stderr.

HTTP limits include a 2 MiB body ceiling (also for chunked uploads), 10 seconds to
read a body, and 16 active request/response streams. These return HTTP 413, 408
and 503 respectively; 503 includes Retry-After. The body deadline does not time
out native analysis. Closing a modern HTTP request cancels its wait and skips
queued work where possible; active native execution still finishes. Legacy
stateless notifications are accepted but are not a separate cancellation channel.
SIGINT/SIGTERM closes the listener, allows up to three seconds for HTTP shutdown,
then releases the worker and its anonymous artifacts.

## stdio compatibility

Existing command-based clients keep working: stdio remains the default when
no transport is selected. It can also be selected explicitly:

```sh
mcp/target/release/dexkit-mcp --transport stdio --allow-root /path/to/apks
```

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

## Input and worker lifetime

Repeat `--allow-root` for multiple directories. The default is the process's
current directory. The adapter checks input metadata, then C++ maps the same
open file descriptor. It does not copy the entire APK to memory or to a
temporary file. Only loaded DEX bytes are retained: stored entries and raw DEX
are copied once, while deflated entries keep their decompressed buffer. The
source APK/DEX is never modified. `open` returns `instanceId`, `byteLength` and
`dexCount`, without computing a whole-file fingerprint. APK input uses consecutive
`classes.dex`, `classes2.dex`, etc.; gaps and decompression failures are errors.
File opens traverse held allowed-directory handles without following replaced
symlinks. Non-regular inputs such as FIFOs are rejected without blocking.

Keep the source unchanged until `open` completes; afterward it may be modified
or deleted without affecting the instance. File size, mtime and ctime are checked
around native loading, and detected changes return retryable `INPUT_CHANGED`.
This is not an atomic filesystem snapshot: concurrent truncation of a mapped
input can terminate the worker and produce `WORKER_EXITED` instead.

Core detects the CPU thread count automatically by default (`--threads 0`, with
a minimum of one). Override it with `--threads N`, for example `--threads 4`.
The same setting controls parallel DEX loading, cache initialization and queries
for each instance, over either transport. `capabilities.nativeThreads` reports
the effective count. This is a Core worker budget, not a process-wide thread cap
or the number of concurrent MCP requests.

APK loading uses Core's shared parallel ZIP extraction and batch `AddImage`
initialization. The adapter checks the total budget before extraction and every
DEX header before initialization, retaining independent DEX bytes in input order.
For duplicate class definitions, Core's declaration lookup keeps the last
logical DEX's definition, independent of thread completion order.

Input budgets are also startup options, shared by both transports:

| Option | Default | Scope |
| --- | --- | --- |
| `--max-input-mib N` | `0` (unlimited) | Complete APK or raw DEX file |
| `--max-dex-mib N` | `512` | Raw DEX bytes, or total uncompressed DEX bytes in one APK |

Set either option to `0` to disable that byte ceiling. For example,
`--max-dex-mib 1024` permits 1 GiB of DEX data without limiting unrelated APK
resources. `capabilities` returns the active byte values in `maxInputBytes` and
`maxDexBytes`; zero means no configured ceiling. The aggregate DEX budget is
checked before any archive DEX is inflated or indexed. Exceeding an enabled
budget returns `LIMIT_EXCEEDED` with the corresponding startup option to adjust.
Platform mapping ranges and supported APK/DEX format constraints still apply.

The SDK handles both the older initialize handshake and the 2026-07-28 protocol.
MCP stdout contains only protocol messages. Native MCP calls run one at a time in
a child process, with Core's internal parallelism and diagnostics routed to stderr. A worker crash
returns `WORKER_EXITED`; restart the server and reopen inputs because all old
handles are invalid. The worker is failure containment, not a security sandbox
or a complete DEX verifier. Cancelling a caller does not interrupt an active Core
query; Core does not expose native cancellation or a general result limit.
For stdio, SDK cancellation notifications stop the caller's wait and skip work
that is still queued. The SDK suppresses cancelled responses.
An unexpected C++ exception also invalidates the worker. Cancellation does not
roll back a completed operation: retained state may still consume its budget.
Close the known instance (or restart the server if its open reply was lost) to
reclaim it immediately; result sets and artifacts also expire automatically.

## Tools and query contract

When a client hides nested input types or shows `query: unknown`, use
`dexkit_get_query_schema` to retrieve the contract through ordinary tool
results. It is available without opening an APK, including while native work is
busy or the worker has failed.

```json
{"tool":"dexkit_find_methods"}
```

Omit `pointer` for an overview with parameter links, rules and sample arguments;
replace the examples' instance placeholder with a real `open` result. Pass
`"pointer":""` for the complete input schema, or copy a JSON Pointer from
`links` to read an exact fragment. Recursive types remain references with
navigation links. Fragment references resolve against the complete input schema
for that tool, so `standalone:false` fragments are not independent validators.
Optional `ifSchemaHash` checks that the document still matches an earlier reply;
on `SCHEMA_CHANGED`, request a fresh overview. The hash covers schema contents,
not native behavior or explanatory notes.

Help uses exactly the final `tools/list` contract and preserves runtime checks.
It addresses lost parameter information without changing the client's schema
compaction policy. Business help replies are limited to 16 KiB and the serialized
MCP tool result (text plus structured content) to 64 KiB, excluding JSON-RPC/SSE
framing. `linksComplete:false` marks an incomplete navigation list.
Oversized sections return `SCHEMA_SECTION_TOO_LARGE` with
child pointers; replies are never cut into invalid JSON. The API contract remains
v1; the help envelope has `discoveryVersion: 1`.

All tools use the stable `dexkit_` prefix without a version in the name.
`serverInfo.version` identifies the program release; `apiMajor` and
`contractRevision` in capabilities/query help identify the business contract.
`schemaHash` fingerprints the input schema only. These values do not provide
automatic tool-version negotiation.

Available tools:

| Tools | Purpose |
| --- | --- |
| `get_query_schema` | Read query contracts, examples and JSON Pointer fragments without an instance |
| `open`, `close`, `capabilities` | Input lifetime, byte length, DEX count and actual capabilities |
| `find_classes`, `find_methods`, `find_fields` | Typed matcher trees, with complete materialized result sets |
| `describe`, `relations` | Metadata, optional annotations/code details and direct relationships |
| `page` | Continue the same retained result set without rerunning Core |
| `smali`, `read_artifact` | Bounded inline output or managed resources/chunks |

For example, after `open` returns an `instanceId`, call:

```json
{
  "name": "dexkit_find_methods",
  "arguments": {
    "instanceId": "returned-instance-id",
    "query": {
      "scope": {"searchPackages": ["com.example"]},
      "matcher": {
        "usingStrings": [{"value": "session expired", "match": "contains"}]
      }
    },
    "pageSize": 50
  }
}
```

The example shows the MCP tool name and arguments; the client/SDK supplies the
JSON-RPC envelope and protocol metadata. `--dump-schema` prints the exact
current input and output schemas. Schemas describe the public DTOs, not raw FBS
JSON. Unknown fields/enums are rejected recursively. Optional conditions must be
omitted instead of set to null; only `parameters.parameters` array slots allow
null, meaning a positional wildcard. An empty parameter list means zero
parameters, and an empty candidate list means no candidates. Empty boolean
`allOf`/`anyOf`/`noneOf` lists are rejected.

Use `className` with Java-style type names and `descriptor` for returned DEX
signatures. String match modes are `equal`, `contains`, `startWith`, `endWith`;
they are not regular expressions. Empty string patterns require `equal`. The
adapter transcodes normal Unicode, including NUL and supplementary characters,
without Unicode normalization or changing Core's matching semantics.

Native collections retain their one-to-one matcher semantics; do not split
conditions that must apply to the same element into separate matchers. Count
ranges require explicit `min`/`max`. Collection matchers use `matchType`;
individual text and flag matchers use `match`. Modifiers and raw access flags
remain separate conditions. `usingNumbers` uses tagged types such as
`{"type":"int64","value":"9223372036854775807"}`; floating query values must
be finite and use Core's numeric comparison, not bitwise equality. Instruction
numeric results expose `opcode` and hexadecimal `rawBits`. Annotation float
bits reflect Core's decoded metadata and preserve the sign of zero. They do not
describe the original variable-length DEX encoding bytes.

`scope.within` accepts exactly one of `entityIds` or `resultSetId`, of the same
kind and instance as the query. `scope.inClasses` accepts class handles to narrow
a method/field search. Entity IDs and cursors are opaque and instance-local, not
persistent identifiers. Default results contain descriptors, flags and source
DEX indices; `select` can request any subset of `descriptor`, `flags`, `source`.
A source DEX index is a logical index within this snapshot, not a ZIP entry name
or an independently stable identifier.

Relations are direct: `invokes`, `callers`, `fieldReaders`, `fieldWriters`,
`declaredMethods`, and `fieldReferences`. The last relation follows the DEX field
ID table and can include referenced, undefined fields; it is not a list of
field declarations. General native method/field searches can similarly include
references in a defined class. Smali may report `NotDefined` for such results.

## Results, resources and limits

Business results use `{"ok":true,"data":...}` or
`{"ok":false,"error":...}` in `structuredContent`, with a text fallback from
the same serialized object. Business errors set MCP `isError`; unknown tools
and malformed protocol requests use protocol errors. Zero matches are successful
empty results. Encoding failures and exceeded limits never become empty success.

Queries finish and retain a complete result set, sorted by descriptor and DEX
identity. `pageSize` controls delivery only. `coverage: "complete"` does not mean
all rows are on the first page. Result sets and artifacts expire after 15 minutes;
closing the instance invalidates them immediately. An expired cursor is an error
and does not silently start a new query.

Current defaults/ceilings include 4 instances, 32 result sets, 50,000 cached
entities and 32 MiB of entity metadata per instance, 200,000 retained result
references, 1 MiB business requests/results, 32 levels/10,000 request JSON nodes
and 64 levels/100,000 response JSON nodes. The configurable DEX byte budget
defaults to 512 MiB per input; there is no default APK container ceiling. A page has
1..500 rows. The native result-copy ceiling is 64 MiB, checked after Core has
constructed the result; these limits are not a total process memory/CPU budget.
Annotation metadata conversion also has a 32-step recursion limit. Exceeding a
response limit returns a business error and keeps the instance available.

`smali` accepts a class/method `entityId`, `debug: "none" | "strict"`,
`maxOutputBytes` (default 1 MiB, maximum 16 MiB) and
`delivery: "inline" | "artifact"`. Inline delivery is limited to 64 KiB. Artifact
results include a `dexkit://artifacts/...` URI and SHA-256; small artifacts can be
read using `resources/read`, while `read_artifact` reads UTF-8 chunks of at most
64 KiB using `startByte`/`nextByte`. Artifacts are private temporary files with a
combined 32 MiB/16-item ceiling, not arbitrary filesystem read/write access.
Native generation failure returns no partial smali, and preserves diagnostic
codes, phases, member identity and offsets. See the [smali contract](./smali.md).

The Rust interface explicitly rejects isolated UTF-16 surrogate metadata. Smali
can still carry those units as ASCII escape sequences. The JVM API decodes DEX
MUTF-8 into Java UTF-16 directly; NUL, emoji and annotation arrays now return their
actual values instead of escaped substitute text. Opaque batch group labels
remain standard UTF-8, and no process-global FlatBuffers codec is changed.

## Development and extension

Run `python3 mcp/test.py` for independent Core interoperability, real stdio pipes,
HTTP sockets, the official SDK HTTP client, and captured-result schema validation. These tests
need the repository's Java/Android setup to assemble smali fixtures. Android
release builds do not link the Rust/JSON/MCP dependencies.

Add optional predicates without changing existing meanings/defaults; add a new
tool for a distinct operation. Update the public DTO, validation,
native mapping, coverage/encoding manifest, semantic tests, capability registry
and contract revision together. New FBS fields/enums/unions without a coverage
classification fail the build. Changes to field types, defaults or semantics
require explicit compatibility review; a coverage list alone cannot prove them.
Keep tool names stable for compatible changes. Breaking public behavior needs
an explicit contract-major change and migration policy; use distinct tool names
or server entry points only when incompatible contracts must coexist. Saved-query
replay, batch-query tools, general regex and transitive call paths are deferred.
