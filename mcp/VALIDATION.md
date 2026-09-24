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

## Large APK input handling

Base: `d67f02c678943bd31931dac0259a4948d4487f0d`.

The adapter now hashes input with a fixed 64 KiB buffer and passes the held file
descriptor to C++. Native code maps that descriptor for the duration of open
and retains only independently owned DEX bytes. Raw/stored DEX data is copied
once; deflated data keeps its decompressed allocation. No full-container heap
copy or temporary APK is created. The source must remain unchanged until open
returns; metadata checks detect ordinary changes but are not an atomic snapshot
or protection against concurrent truncation of a mapped file.

The input-file ceiling defaults to disabled. Both `--max-input-mib` and
`--max-dex-mib` are startup options, with zero disabling the respective byte
ceiling; the DEX default remains 512 MiB. C++ checks the aggregate DEX budget
before any archive DEX is inflated/indexed. These are not total RSS/CPU limits.

Executed on macOS arm64:

- All 13 workspace unit functions and 11 independent interop tests pass.
- All 15 stdio and 14 HTTP tests pass in debug and release. New regressions cover
  a 257 MiB resource entry plus two stored/aligned DEX entries, exact whole-input
  fingerprints, successful query/smali after the source APK or raw DEX is
  truncated/deleted, custom budgets, zero/raised budgets, CLI errors and failed
  opens not consuming instance slots. Input hashing crosses buffer boundaries
  and detects changes through the held descriptor.
- The aggregate preflight regression uses a deliberately invalid first DEX and
  a later entry declaring more than 512 MiB. Default policy returns a limit error
  before parsing the first DEX; raised/disabled policy reaches input validation.
  This verifies control flow without allocating the declared large data; it does
  not claim execution of a valid DEX payload larger than 512 MiB.
- The generated contracts validate 110 stdio and 69 HTTP request/result cases
  across all 12 tools, including effective capability budgets.
- The real QQ 9.3.55 APK (389727209 bytes) opens over release HTTP using defaults:
  all 41 DEX entries, 411819364 uncompressed DEX bytes. A targeted query for a
  declared class in `classes41.dex` returns `source.dexIndex = 40`, produces
  2454 bytes of smali and closes successfully. An independent Python SHA-256
  agrees with the whole-APK fingerprint; the input is not modified.
  Open took 2.229 seconds with the final release, not a portable performance claim.
  Worker RSS was 597488 KiB after open and 222448 KiB after close; these are
  point samples, not peak memory or a promise that allocators return all pages.
- Workspace clippy with warnings denied, formatting and diff checks pass.
  Host CMake/JAR/JVM and Android release Gradle checks pass (113 tasks: 15
  executed, 98 up-to-date). JVM tests are up-to-date, so the earlier 143-test
  result remains baseline evidence, not a newly forced run.
- Frozen Yarn installation and the 28-page documentation build pass.

The macOS arm64 release is 6616896 bytes, 19120 bytes larger than the query-help
baseline. SHA-256:
`0e80eb56d3d6de41dea2ebef7704dccc920d959f11773659dab940a644711731`.
It still links only the same macOS system libraries. No dependencies or
Core/JNI/FBS/Android production sources changed; the Android link graph is unchanged.

The Pro source-review snapshot contains the actual delta and ownership context:
265811 bytes, SHA-256
`e423e3d7ca5c9eecb23dcd3b4c026c8ddc36a9b3ec98a1b43f3cd63b9d37d135`.
Pro verified all 19 complete file blocks and the 37 matching diff hunks, and
found no substantive blocker or input-mapping ownership escape. It recommended
checking source metadata even when native loading returns an ordinary error,
actually destroying the original raw DEX contents in the HTTP lifetime test,
and clarifying the source-stability requirement in the open tool description.
Those local refinements were applied after review. A related local regression
first reproduced incorrect classification when a file is truncated to zero
before mmap; checking expected size before rejecting an empty mapping fixes it.
The final unit/clippy checks, release HTTP/stdio suites, captured-schema checks
and real QQ open/query/smali/close all pass after these refinements. Pro's review
covers the supplied source; the post-review refinements and execution results
were validated locally, not by another Pro test run.

## Stable tool names before publication

Base: `10484393c0df17065cec376bfcb96bb83e0b9192`.

All 12 tools now use the stable `dexkit_` prefix, including the query-help tool
and its three tool selectors. Catalogue names, native-worker dispatch,
capabilities, instructions, resource diagnostics, SDK examples and client tests
use the same names. As requested for the unpublished implementation, no legacy
aliases were added. API major 1, contract revision 1.0 and the program version
remain separate metadata; query parameters and semantics did not change.

Executed validation:

- `mcp/test.py` passes: 13 workspace unit functions, 11 interop tests, 15 stdio
  tests and 14 HTTP tests, including the official SDK client. The generated
  schemas validate 110 stdio and 69 HTTP request/result cases across all 12 tools.
- The installed Codex 0.155.0-alpha.9 check passes in Code Mode and native tool
  mode with the renamed declarations/selectors. It uses a local scripted model
  endpoint; Code Mode discovers help and executes a real query from its returned
  example. This checks client integration, not autonomous model performance.
- Both workspaces pass clippy with warnings denied and formatting checks.
  Frozen Yarn installation and the 28-page documentation build pass.
- The release catalogue contains exactly the 12 expected names and the updated
  query-tool enum; the helper input schema is 546 compact JSON bytes. The official
  SDK also passes discovery/open/find/close against the release HTTP executable.
- Source/example/documentation searches find no remaining versioned tool names.
  No Core/JNI/FBS/Android production sources changed; their previous checks remain
  baseline evidence for this naming-only update.

The macOS arm64 release remains 6616896 bytes. SHA-256:
`09851feb6da9582a78c99e4a238e69090d8b7638c1de7bdd1ce10417ee223622`.

## Remove unused input fingerprints

Base: `2c641a6f36d68006b5b4716aad0ac6a57a431073`.

This update removes the whole-file SHA-256 preparation pass and the open reply's
fingerprint field, superseding that behavior in the earlier input-handling record.
Open now returns only `instanceId`, `byteLength` and `dexCount`. File-size budgets,
held-descriptor mapping, before/after native-loading metadata checks and independent
DEX ownership remain in place. Query-schema and smali-artifact hashes are retained.

The existing suite passes: 13 workspace unit functions, 11 interop tests, 15 stdio
tests and 14 HTTP tests. Actual published schemas validate 110 stdio and 69 HTTP
request/result cases across all 12 tools. The large stored-APK regression checks
the exact three-field open reply and query/smali after source truncation/deletion;
input budget and change-detection regressions still pass. Workspace clippy with
warnings denied, formatting, diff checks and the 28-page docs build pass.

The release schema contains exactly those three Opened properties and required
fields. For the real 389727209-byte QQ APK, old/new release binaries were run in
alternating order three times each on macOS arm64, with warm filesystem cache and
a fresh server/worker per run. Discovery and worker readiness were outside the
timed HTTP open request. Results in milliseconds:

| Build | Run 1 | Run 2 | Run 3 | Median |
| --- | ---: | ---: | ---: | ---: |
| Before | 2118.305 | 2019.577 | 2044.852 | 2044.852 |
| After | 865.582 | 874.386 | 871.823 | 871.823 |

The measured median decreased by 57.4%. Each new-build run opens all 41 DEX entries,
queries the declared class in logical DEX 40, produces 2454 bytes of smali and
closes successfully. This is a local warm-cache result, not a cold-cache or
cross-platform performance guarantee. Native loading is still serial; no parallel
loader or full-cache initialization was added.

The macOS arm64 release is 6616496 bytes (400 fewer than the previous release).
SHA-256: `62d524459ccdce9283324977fff7280a80a3d1bcf0e614955a5d3d278a129fd1`.
No C++ Core, JNI, FBS or Android sources changed; their earlier checks remain
baseline evidence.

## Automatic threads and shared Core loading

Base: `3d4742b3a5acde450a1b2979eaa7981e8167b226`. `--threads N` now defaults to
Core's normalized hardware thread count; zero selects automatic mode. The
effective value is forwarded to every native instance and reported as
`capabilities.nativeThreads`. Native requests still run one at a time.

Core's existing parallel ZIP extraction is now shared by `AddZipPath` and MCP.
Both paths use batch `AddImage` for initialization. MCP keeps aggregate budget
and entry-number preflight, checks every extracted DEX header before indexing,
and asks the batch extractor to detach stored entries from the input mapping.
Batch initialization now observes task futures instead of silently discarding
construction exceptions. No JNI signatures, FBS definitions or Android
dependencies changed.

Executed locally:

- `mcp/test.py`: 11 interop tests, 13 workspace unit functions, 16 stdio tests,
  14 HTTP tests, and 144/69 real request/result schema cases pass.
- Automatic/1/3-thread startup, mixed stored/deflated multi-DEX order, source
  replacement after open, and recovery after a bad later DEX are covered.
  The mixed-archive and large stored-APK lifetime cases also pass in release.
- CMake/CTest: the new ZIP batch ownership/order/failure checks and existing
  checked smali reader pass. These assert data/lifetime behavior, not OS thread
  scheduling or timing. The test fixture initially used the wrong EOCD field
  name; correcting it to `cd_offset32` resolved the compile error.
- `bash gradlew :dexkit:cmakeBuild :dexkit:jar :dexkit:test
  :dexkit-android:assembleRelease` passes (final run: 18 executed, 95 up-to-date).
  All 143 JVM tests actually ran, with no failures, errors or skips.
- Release build, workspace clippy with `-D warnings`, formatting, diff checks,
  and the 28-page documentation build pass.

The same 389727209-byte, 41-DEX QQ APK was measured with a fresh HTTP server and
worker per run. Discovery/startup is outside the timer; filesystem cache is warm.
Three runs per setting alternate forward/reverse order, with no concurrent local
build or test job during the recorded measurement:

| Setting | Median open (ms) |
| --- | ---: |
| Previous serial loader | 922.130 |
| New `--threads 1` | 895.085 |
| New `--threads 2` | 485.246 |
| New `--threads 4` | 297.292 |
| New default (8 threads on this host) | 244.292 |

The final default median is 73.5% lower on this workload. Every new-build run opens
all 41 DEX entries, finds the class in logical DEX 40, produces 2454 bytes of
smali and closes. These are local results, not a general scaling guarantee.

The macOS arm64 executable is 6651488 bytes (+34992); SHA-256:
`98a72b0473c7db67004fbf6b80d3ef9bc38eb1f85276087fb283375b7b68edfc`.
Stripped Android shared-library sizes from the same local release pipeline:

| ABI | Before | After | Delta (bytes) |
| --- | ---: | ---: | ---: |
| arm64-v8a | 445904 | 447136 | +1232 |
| armeabi-v7a | 301604 | 302756 | +1152 |
| x86 | 493772 | 495964 | +2192 |
| x86_64 | 477704 | 479672 | +1968 |

The first source review (399089-byte snapshot, SHA-256
`dc58746c9f32936a619643fda535620237cc8e920a6af7c2d3c20c351d67e0e7`)
found no substantive blocker. Its duplicate-class observation was also confirmed
locally: twelve parallel opens of eight same-name DEX definitions selected indices
`[0,7,7,7,7,5,7,5,6,7,6,7]` for an exact class lookup. Core now keeps the greatest
logical DEX ID under the existing registration lock, preserving the previous
serial loader's last-DEX precedence. The deterministic native regression models
both registration orders explicitly: it failed before the fix and passes after.
The MCP regression now checks exact lookup as well as stable enumeration indices.

The batch helper also rejects zero alignment consistently in both ownership
modes. The default-thread-count C ABI catches unexpected C++ exceptions, including
allocation by the temporary Core's standard containers, before they can unwind
through Rust. These final changes are included in all results above.

Loading is not transactional on unexpected Core construction exceptions: direct
C++ callers must discard a partially initialized instance. MCP terminates its
worker instead of returning that state. Ordinary ZIP/DEX-header failures occur
before initialization and remain recoverable; the tests do not assert arbitrary
construction-exception recovery. Core's ordinary AddZipPath retains its existing
stored-entry borrowing policy; the independent-input guarantee applies to MCP's
explicit detached mode. No new ThreadPool recovery or Core constructor error
reporting contract is claimed.

The 28284-byte follow-up delta (SHA-256
`79fc765b3b536d77735c7a9d63478d0257da03a3cf1d9e91a23984078878ee1f`)
passed focused Pro re-review with no substantive blocker. Pro confirmed the
duplicate-class precedence, zero-alignment check and C ABI exception boundary.
The source was unchanged after that snapshot except for plan/validation records.
Both reviews are source review, not Pro-executed builds or performance tests.
