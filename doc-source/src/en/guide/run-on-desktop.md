# Run on desktop platform

> Starting from version `1.1.0`, DexKit supports running on desktop platforms without
> the need for packaging as an APK for testing on Android.

## Install environment

The basic runtime environment requires gcc/clang, cmake, and ninja/make.

### Windows

`Windows` users can use [MSYS2](https://www.msys2.org/) to set up the runtime environment.
Since all Windows systems are currently 64-bit, we use `mingw64.exe` for dependency installation:

```shell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

After installation, we need to add the `mingw64/bin` directory to the environment variables
for future use.

::: warning warning
DexKit will use `ninja` as the default build system by default. If you need to use `make`
in mingw for building, you need to execute `pacman -S mingw-w64-x86_64-make`, after installation,
you need to rename `msys64\mingw64\bin\mingw32-make.exe` to `make.exe` or add it as a shortcut,
otherwise the build will fail due to `gradle-cmake-plugin` not finding the make command.
Additionally, delete `generator.set(generators.ninja)` in `:dexkit/build.gradle`,
or modify it to `generator.set(generators.unixMakefiles)`.
:::

### Linux

On Linux, you normally only need to install `ninja` to use it.

### MacOS

It's recommended to use [HomeBrew](https://brew.sh/) for dependency management.

```shell
brew install cmake ninja
```

## Clone DexKit

```shell
git clone https://github.com/LuckyPray/DexKit.git
```

## To begin using

Execute the submodule `:main` to perform testing.

```shell
gradle :main:run
```

## Native MCP server

The experimental `dexkit-mcp` executable provides a standard **stdio** MCP entry
point using Rust and the existing C++ Core. It needs no JVM at runtime. This
server's build currently targets macOS and Linux; the Windows/JVM instructions
above do not imply a Windows build of the Rust server is available.

Build with Rust 1.88+, CMake, a C++20 compiler and zlib development files:

```sh
cargo build --release --manifest-path mcp/Cargo.toml --locked
mcp/target/release/dexkit-mcp --allow-root /path/to/apks
```

Configure your MCP client to launch that executable with absolute paths:

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

Repeat `--allow-root` for multiple directories. The default is the process's
current directory. Input files are read into an immutable snapshot; the source
APK/DEX is never modified. The input fingerprint covers the complete original
file, including the full container/archive. APK input uses consecutive
`classes.dex`, `classes2.dex`, etc.; gaps and decompression failures are errors.
File opens traverse held allowed-directory handles without following replaced
symlinks. Non-regular inputs such as FIFOs are rejected without blocking.

The SDK handles both the older initialize handshake and the 2026-07-28 protocol.
MCP stdout contains only protocol messages. Native analysis runs serially in a
child process with its diagnostics permanently routed to stderr. A worker crash
returns `WORKER_EXITED`; restart the server and reopen inputs because all old
handles are invalid. The worker is failure containment, not a security sandbox
or a complete DEX verifier. Cancelling a caller does not interrupt an active Core
query; Core does not expose native cancellation or a general result limit.
The SDK cancellation token stops the caller's wait and skips work that is still
queued. The SDK suppresses responses for requests it has already cancelled.
An unexpected C++ exception also invalidates the worker. Cancellation does not
roll back a completed operation: retained state may still consume its budget.
Close the known instance (or restart the server if its open reply was lost) to
reclaim it immediately; result sets and artifacts also expire automatically.

### Tools and query contract

All tools use the `dexkit_v1_` prefix:

| Tools | Purpose |
| --- | --- |
| `open`, `close`, `capabilities` | Input lifetime, fingerprints and actual capabilities |
| `find_classes`, `find_methods`, `find_fields` | Typed matcher trees, with complete materialized result sets |
| `describe`, `relations` | Metadata, optional annotations/code details and direct relationships |
| `page` | Continue the same retained result set without rerunning Core |
| `smali`, `read_artifact` | Bounded inline output or managed resources/chunks |

For example, after `open` returns an `instanceId`, call:

```json
{
  "name": "dexkit_v1_find_methods",
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

### Results, resources and limits

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
and 64 levels/100,000 response JSON nodes,
256 MiB input files and 512 MiB total inflated DEX data per archive. A page has
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

### Development and extension

Run `python3 mcp/test.py` for independent Core interoperability, real stdio pipes
and validation of captured results against the advertised schemas. These tests
need the repository's Java/Android setup to assemble smali fixtures. Android
release builds do not link the Rust/JSON/MCP dependencies.

Add optional predicates without changing existing meanings/defaults; add a new
tool for a genuinely different operation. Update the public DTO, validation,
native mapping, coverage/encoding manifest, semantic tests, capability registry
and contract revision together. New FBS fields/enums/unions without a coverage
classification fail the build. Changes to field types, defaults or semantics
require explicit compatibility review; a coverage list alone cannot prove them.
Breaking public behavior needs a new `dexkit_v2_` contract. HTTP, saved-query
replay, batch-query tools, general regex and transitive call paths are deferred.
