# Native MCP validation record

## Environment and scope

Validated on macOS arm64 with Rust/Cargo 1.98.1, Apple Clang/CMake, zlib,
JetBrains Runtime 17.0.6, the repository Gradle wrapper, Android SDK/NDK,
and Yarn 1.22.19. The source base is
`76d2dca3ac5fbc098ff09db601e91b1be5f2e0ee` on the `smali` branch.
The implementation is developed directly on `mcp`.

The MCP executable statically links Core and needs no JVM at runtime.
`otool -L` lists only macOS system libraries: libc++, libz, libiconv and
libSystem. Java/Android tooling is needed for the existing regression suites
and for assembling the smali fixtures, not for running the MCP server.
Linux linking is configured but has not been executed in this environment;
Windows is not supported by this first native adapter. Rust 1.88 is the declared
minimum; the executed compiler was 1.98.1, not an MSRV verification run.

## Executed checks

- Independent production-adapter compatibility suite: 11 tests pass, including
  all number union variants, nested annotation union vectors, an independent
  C++ string-query producer, NUL/supplementary Unicode, explicit isolated-unit
  errors, empty candidates, parameter wildcards, declared methods versus
  references, signed annotation zeros, ownership, paging and smali artifacts.
- Library unit checks: codec boundaries, allowed-root/FIFO handling, replaced
  symlink components, preflight of later oversized pages and expiry cleanup.
- Binary unit checks: native C stdout isolation and per-line input limits, plus
  the isolated test-process helper. The oversized-frame test initially exposed
  an AsyncRead contract violation; restoring ReadBuf's prior filled length
  before returning an error fixed it, and the regression passes.
- Ten subprocess pipe tests exercise legacy initialization and the 2026-07-28
  SDK flow, all 11 tools, resources, Unicode, malformed tool inputs, APK/DEX
  loading, paging, close, and explicit worker termination. A 70-level encoded
  annotation returns
  LIMIT_EXCEEDED and the same instance can still find its class. Cancelling
  queued opens no longer consumes hidden instance slots; the regression fails
  against the pre-fix release. A near-2-MiB request whose numeric normalization
  grows its JSON is rejected before private IPC, leaving the instance usable.
  Artifact creation leaves no named temporary output in the configured test temp directory.
- Schemas returned by the running server validate 44 captured request/result
  pairs across all 11 tools. Deliberately invalid structural inputs are checked
  as invalid, and business error outputs are validated too. This is sampled
  behavioral coverage, not proof of every matcher or every DEX layout.
- `cargo clippy` for both workspaces, all targets, with warnings denied; Rust
  formatting checks and `git diff --check` pass.
- `bash gradlew :dexkit:cmakeBuild :dexkit:jar :dexkit:test
  :dexkit-android:assembleRelease` passes. The JVM suite reports 143 tests with
  zero failures/errors/skips; Android release builds cover all four configured
  ABIs. Existing compiler/deprecation warnings remain.
- `yarn install --frozen-lockfile` and `yarn docs:build` pass. The existing
  Browserslist database warning does not fail the build.

Commands for reproducing the adapter checks are in README.md and
`../tests/interop/planus/README.md`. No generated build outputs are committed.

## Size and lifetime

The macOS arm64 release uses thin LTO, one codegen unit and debug-info stripping.
The stdio baseline executable at `d039774` was 4968880 bytes (4.739 MiB);
it passed the ten real-pipe tests in release mode. Its SHA-256 is
`e45f8f0f62727ffdf12f623f9b648d53c630fcc748f320e80420f3ec8576aad4`.
Test-only JSON Schema validation dependencies do not enter this executable.

Android stripped library sizes before/after the shared signed-zero producer fix:

| ABI | Before (bytes) | After (bytes) | Delta |
| --- | ---: | ---: | ---: |
| armeabi-v7a | 301636 | 301604 | -32 |
| arm64-v8a | 445936 | 445904 | -32 |
| x86 | 493788 | 493772 | -16 |
| x86_64 | 477736 | 477704 | -32 |

These are local release artifacts with identical build settings, not a portable
size guarantee. The Rust workspace, C ABI adapter, SDK and JSON dependencies
are outside the Android link graph.

Artifact files are anonymous/unlinked and owned by the worker. Closing an
instance, expiry, normal shutdown or worker death releases their file handles;
there is no named smali output that needs a destructor to unlink it.
Expiry is checked on service access, rather than by a background timer.

The worker contains crashes and unexpected C++ exceptions; it is not an OS
sandbox or a full malformed-DEX verifier. No native cancellation, query timeout
or native result-count limit is claimed. Cancellation can complete with retained
state; callers can close a known instance, wait for result/artifact expiry, or
restart if an open response was lost. These limits are stated in the guide.

## Pro source review

Authorized conversation:
https://chatgpt.com/c/6aad8202-1ee4-83e9-983b-fc6d0578d7fc

The design review accepted Planus with explicit byte-string adaptation and local
JVM codecs. The first source snapshot was 248378 bytes (41 file blocks), SHA-256
`ede2c3bd0d736216ebb9dba9c9cc0f88598a49e3f00c660839c1996a99082be7`.
It was not approved as complete. Confirmed findings were fixed: native quota
mutation before successful publication, misleading declaration relations,
undeliverable later pages, annotation signed zero, public execution paths that
bypassed validation, exception reuse, and path-opening races. The manifest's
classification and generated string-field structural checks were also tightened.

The complete second snapshot was 374388 bytes (56 file blocks), SHA-256
`ab0992efe94dec1cb4b8545d99e53d6a68834625dff1ac9cfe87634b7c208aec`.
It includes SDK/worker code, complete source files, Kotlin changes, and generated
accessor excerpts. Pro closed the first-round findings and identified two new
integration P1 issues: named artifact cleanup on worker death, and metadata
reply depth exceeding the parent's JSON parser limit. A P2 identified the
unconnected SDK cancellation token. All were fixed locally and targeted tests
pass. Output schema success/failure flags were also constrained with const
values and inverted-flag negative checks.

Additional local regression work fixed the AsyncRead error path and validated
requests before reserialization/IPC; the latter previously turned an oversized
query into WORKER_EXITED. Two annotation identity entries were corrected to
wire-only.

The final delta was 203785 bytes (14 complete files plus diffs), SHA-256
`82c0a323e5c8e1e295de94f7ed69653df189ce7e33d243fc3a29bf4e06df30fc`.
Pro verified its hashes and correspondence to the second snapshot, closed both
P1 issues and the P2, and found no new substantive blocker in this delta.
The reviewed source combination is accepted for the declared experimental
first release. This is source acceptance, not a claim of Pro-executed tests or
validation of platforms outside the local environment.

No implementation changed after that review; only the review/plan records were
finalized before the three scoped commits.

Pro's findings are source-review evidence. Build/test outcomes above are local
execution results, not tests performed by Pro.


## Streamable HTTP completion

The prior three-round acceptance applied to the stdio implementation only. The
user clarified that the intended transport also included Streamable HTTP. This
follow-up starts from `d039774954a337d018ef459e0b8c0cbf5a7c8fe8` and corrects
that omitted scope; prior stdio approval is not HTTP approval.

The official rmcp 3.4.0 Streamable HTTP service now serves `/mcp` with
`--transport http`. It supports modern request metadata and legacy initialization
without transport session IDs. A shared worker owns the explicit application
instances, so state survives new connections and repeated initialization. The
local single-user scope, shared quotas and lack of remote authentication are
stated in the independent English/Chinese MCP guides and README.

Implemented transport boundaries include loopback-only binding, exact Host/Origin
allowlists, a streamed 2 MiB request-body limit, a ten-second body read deadline,
16 active HTTP requests/response streams, SDK disconnect cancellation and bounded
SIGINT/SIGTERM shutdown. The body timeout wraps body reading only; timing the full
SDK response would incorrectly impose a native query timeout for modern MCP.

Current checks on macOS arm64:

- All 11 independent interop tests and 10 workspace unit test functions pass.
- All 10 existing stdio pipe tests still pass (44 schema samples).
- 12 real HTTP tests pass, covering all 11 tools, new connections retaining
  handles, Unicode, resources/chunks, legacy initialize, concurrent ownership,
  body/header limits, queue cancellation on disconnect, worker failure and
  shutdown. The stalled-body test gets HTTP 408 while a blocked native request
  continues waiting, then succeeds after the worker resumes.
- 35 HTTP request/result pairs validate against the actual tool schemas, including
  inverted success-flag rejection. The official rmcp HTTP client performs modern
  discovery and open/find/close against the actual executable with proxies disabled.
- The HTTP suite also passes against the release executable.
- Workspace clippy with warnings denied, Rust formatting and documentation build
  pass; VuePress now renders 28 pages including the independent MCP guides.
- Host CMake/JAR/JVM and Android release Gradle checks pass. Unchanged JVM/Android
  tasks were up-to-date; the 143-test JVM result remains baseline evidence rather
  than a newly forced test run. No Core/JNI/shared schema/Android sources changed.

The first HTTP source snapshot was 232139 bytes (21 complete file blocks),
SHA-256 `605abed0b58ff1b85f4bc55a024de4a3a3fed6da0ad4d58385d36e8eca5b4472`.
Pro verified the source and found one P1: rmcp 3.4.0 permanently caches unknown
tool names in a shared schema cache. Local inspection confirmed the unbounded
negative cache. The HTTP router now creates a request-scoped SDK service while
sharing the immutable catalogue, application worker, configuration and global
stream semaphore. No SDK fork or native instance recreation is involved.

A production-router regression counts unknown schema lookups across requests,
including repeated names. It passes with request-scoped routing and fails on
the third request with the prior persistent service (two lookups instead of
three). The same regression checks that Mcp-Name/body mismatch still returns
the SDK's HTTP 400 / JSON-RPC -32020 error.

The HTTP baseline at `efbd390` is 6397696 bytes (6.101 MiB),
an increase of 1428816 bytes (1.363 MiB) from the stdio baseline above. SHA-256:
`e0f245dedc591af50bc6677c855039c49085176d58b9c5b20d1e615e435006f3`.
It still links only macOS system libraries. The official HTTP client and
regression helper dependencies are test-only. Android library sizes are
unchanged by this transport work.

The final cache correction snapshot was 34165 bytes (three complete files plus
diffs), SHA-256
`eb6aa23656d36b65a6e312359f97be2e6abf9cb4b761a1aef16ecfdab6229911`.
Pro verified this delta against the first HTTP snapshot, closed the cache P1,
and found no new substantive blocker in the request-scoped transport wrapper.
The accepted source combination covers Streamable HTTP within the declared
local, trusted-user, stateless experimental scope, as well as the retained stdio
entry. This was source review, not Pro-executed builds or tests.
Only these review/plan records were finalized after acceptance.

## Query contract discovery

Base: `efbd3901046d79df729c8bd22b357ca228dc9e7c`. A separate client diagnostic
confirmed that Codex 0.155.0-alpha.9 compacts input schemas above a 5000-byte
normalized budget. The raw DexKit contracts were intact; model-visible query
types were lost before TypeScript rendering. This feature exposes the same
contracts as bounded ordinary tool results. It does not fix the client's
compactor or change native query semantics, APK budgets or transport protocols.

Pro's design consultation recommended an overview, exact JSON Pointer fragments
and optional complete-document retrieval. The implementation follows that design,
inlines Select from its generated definition, and keeps discovery in the parent.
MCP capabilities describe all 12 advertised tools through a dedicated output DTO;
the analysis library retains its existing 11 executable operations.

Executed validation:

- Two new Rust regressions cover original-document equality, all current
  definition fragments, navigation destinations, examples, enum equivalence,
  malformed selectors, stale hashes, escaped pointers and oversized replies.
  All 12 workspace unit functions and 11 independent interop tests pass.
- All 11 stdio and 13 HTTP tests pass. They exercise discovery before opening
  an input, execute all five returned examples on the real fixture, and check
  help while the native worker is stopped or dead. The actual advertised schemas
  validate 72 stdio and 63 HTTP request/result samples across 12 tools.
- The same stdio and HTTP suites pass against the release executable.
- `test_codex_discovery.py` runs the installed `codex-cli 0.155.0-alpha.9`
  against a local scripted model endpoint with user configuration/rules ignored.
  It verifies both code-mode and native tool definitions, the small help input
  and inline Select enum, and equality of the complete schema delivered in a
  subsequent model request with the binary's published schema. The code-mode
  sequence executes a real nested parameter query from the returned examples
  and closes its instance. No real model/API call is made. This is client
  integration evidence, not autonomous model tool-selection or success-rate
  evidence. Both modes pass with the release executable, including explicit
  Select enum assertions in both modes.
- The helper input schema is 555 compact JSON bytes. Business help JSON is
  bounded to 16 KiB, including errors; the serialized CallToolResult is checked
  against 64 KiB, excluding JSON-RPC/SSE framing. Query schemas retain their
  complete references and semantics.
- Workspace clippy with warnings denied, formatting, diff checks and the
  28-page bilingual documentation build pass. No Core/JNI/schema/Android files
  changed, so the prior JVM/Android results remain baseline evidence.

The macOS arm64 release is 6597776 bytes (6.292 MiB), an increase of 200080 bytes
from the HTTP baseline. SHA-256:
`2bcfa7031c7fb11181d4797f4c4d806db4550df3f5af8082c4d0830f450fb74a`.
This change does not enter the Android link graph.

The completed source snapshot was 146373 bytes, SHA-256
`3ec075286dc2677c5f7da52c5381f08eac4a378924b1c733a4ca4ad951b548ec`.
Pro verified all 13 complete source/test/guide blocks against their hashes and
reviewed the other five files as diffs. It accepted the discovery implementation
with no substantive blocker. Two nonblocking precision suggestions were adopted:
explicitly check Select in Code Mode as well as native mode, and clarify that
the 64 KiB check covers CallToolResult rather than the entire network frame.
The installed-Codex check and documentation build passed again after these
test/documentation refinements. Runtime source was unchanged after acceptance.
Pro reviewed source; execution and binary measurements above were performed locally.
