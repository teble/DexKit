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
The current executable is 4968880 bytes (4.739 MiB); it also passes
the ten real-pipe tests in release mode. Its SHA-256 is
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
