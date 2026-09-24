# Native MCP implementation plan

Base: `76d2dca3ac5fbc098ff09db601e91b1be5f2e0ee` (`smali`).
Work directly in the `mcp` branch; publish to `teble:mcp` after source review.

## Scope

- An independent Rust workspace: `dexkit-sys`, `dexkit-rs`, `dexkit-mcp`.
- Static Core linkage and a private synchronous C ABI with explicit ownership.
- Typed JSON queries and strict validation, generated input/output schemas,
  explicit coverage of native schema additions, and versioned tool names.
- Open/close/capabilities, class/method/field search, entity details and direct
  relationships, complete materialized result paging, bounded smali output and
  managed artifact access over standard MCP transports. The initial stdio-only
  delivery omitted the requested Streamable HTTP entry and is corrected below.
- Normal Unicode including embedded NUL and supplementary characters. Decode
  native MUTF-8 before exposing UTF-8; encode queries in the reverse direction.
  Reject isolated UTF-16 surrogates explicitly in the Rust public contract.
- Repair the demonstrated Kotlin query/result encoding boundary using local
  helpers, without changing the process-global FlatBuffers codec.
- Keep the standalone Planus compatibility experiment in `tests/interop/planus`.
  Kotlin regressions stay with the JVM tests.
- No Android dependency on Rust, JSON, schema generators or MCP SDKs.

## Steps

1. [x] Inspect baseline and preserve the existing unrelated Main.kt edits.
2. [x] Relocate the independent interoperability experiment.
3. [x] Implement and verify typed native binding and Unicode adaptation.
4. [x] Implement public DTOs, query mapping and bounded analysis service.
5. [x] Add SDK stdio transport, resources, documentation and end-to-end checks.
6. [x] Run Rust, JVM, Android and documentation checks; measure release size.
7. [x] Submit concrete source to the authorized Pro conversation, assess and
   fix substantive findings, and resubmit changed source until no blocking
   issue remains in the declared scope.
8. Publish three scoped commits to `teble:mcp`; Git and the goal completion
   report record the final remote revision.

## Invariants

- Unknown conditions must never be silently discarded.
- Empty candidate sets stay empty; parameter wildcards preserve positions.
- IDs belong to an instance; closed or expired state is an error.
- Paging does not imply native result limits, streaming or cancellation.
- Native logging must never write onto the MCP protocol stream.
- A failed decode or exhausted budget must not become a successful empty query.

## Review

The authorized Pro conversation is
https://chatgpt.com/c/6aad8202-1ee4-83e9-983b-fc6d0578d7fc.
The design consultation confirmed Planus with explicit string adaptation.
The first source review covered the foundation snapshot (SHA-256
`ede2c3bd0d736216ebb9dba9c9cc0f88598a49e3f00c660839c1996a99082be7`).
Confirmed findings were assessed and fixed: native ID quota mutation on failure,
reference/declaration confusion, preflight of every page, annotation signed zero,
unchecked public typed entry points, native exception lifetime, and path-opening
races. Generated string-field coverage now has structural build assertions.
The second source review closed the foundation findings and identified artifact
cleanup, reply depth, and cancellation integration issues. Anonymous files,
bounded metadata/replies and SDK token handling now address them. The final delta
(SHA-256 `82c0a323e5c8e1e295de94f7ed69653df189ce7e33d243fc3a29bf4e06df30fc`)
was reviewed with no remaining substantive blocker in the declared first-release
scope. All prior P1/P2 findings are closed.
Pro performs source review; local build/test evidence is recorded separately.


## Streamable HTTP correction

Base: `d039774954a337d018ef459e0b8c0cbf5a7c8fe8`; continue on `mcp`.

1. [x] Add the official SDK Streamable HTTP service at `/mcp`, selected by
   `--transport http --listen 127.0.0.1:7331`. Keep existing stdio startup working.
2. [x] Reuse one bounded native worker for the local single-user server. Business
   instances live until explicit close/server exit, independent of TCP requests.
   Use stateless HTTP routing for modern and legacy clients; do not mint protocol
   session IDs. Legacy initialize remains supported by the SDK.
3. [x] Enforce loopback binding, Host/Origin validation, request-body and active
   HTTP stream limits, and bounded shutdown. Use SDK SSE responses and
   cancellation rather than implementing JSON-RPC framing ourselves.
4. [x] Exercise real HTTP clients, cross-request handles, Unicode, resources,
   error/limit paths, cancellation, legacy initialization and process shutdown.
   Re-run the existing stdio/interop checks and measure native executable size.
5. [x] Give MCP its own Chinese/English guide and navigation entry, with HTTP
   startup/client configuration and explicit instance/session lifetime semantics.
6. [x] Obtain Pro review of the actual transport delta and fix verified findings.
   The SDK's unbounded unknown-tool cache is now request-scoped; the production
   router regression fails against the previous persistent service and passes
   after the fix. Pro accepted the final delta with no substantive blocker.
7. Publish the corrected implementation to `teble:mcp`; Git and the completion
   report record the final remote revision.

This local server has one trusted-user state space and one set of allowed roots.
It does not treat MCP clientInfo as authentication or promise per-client data
isolation. The existing instance/entity ownership checks remain unchanged.

## Query contract discovery compatibility

Base: `efbd3901046d79df729c8bd22b357ca228dc9e7c`; continue on `mcp`.

1. [x] Consult Pro using the verified Codex 5000-byte schema-compaction evidence
   and current catalogue/DTOs; settle a bounded contract-discovery interface.
   Use overview when pointer is omitted, full schema for an empty pointer, and
   unchanged fragments with JSON Pointer navigation otherwise. Keep the schema
   hash as an optional precondition and handle help in the parent.
2. [x] Implement discovery from the same generated query schemas, preserve
   query semantics and inline small enums where useful. Keep help available
   without an APK and independent of native work.
3. [x] Verify schema navigation, examples, malformed requests and real HTTP/stdio
   queries; exercise model-visible declarations with the diagnosed Codex client.
4. [x] Update bilingual guides, measure the executable and submit the actual
   source delta to Pro. The source review found no substantive blocker; the
   suggested Code Mode Select assertion and result-budget wording were added
   and verified without changing runtime code after review.
5. Publish to `teble:mcp`; Git and the completion report record the remote revision.

This follow-up addresses query discoverability. It does not change APK size
budgets, native execution, client binaries or the existing transport modes.

## Large APK input handling

Base: `d67f02c678943bd31931dac0259a4948d4487f0d`; continue on `mcp`.

1. [x] Inspect both input limits, native ownership and the 372 MiB APK case.
2. [x] Replace full-container heap copies with direct native mapping of the held
   input descriptor. Keep only owned DEX bytes, preserving allowed-root opening,
   whole-input fingerprints and independence from changes after open completes.
3. [x] Remove the default container size ceiling; make input and total DEX byte
   budgets configurable at startup, including explicit unlimited settings.
   Check aggregate DEX size before loading any archive entries.
4. [x] Exercise stored/deflated archives, limits, lifetime and HTTP/stdio;
   open all 41 DEX entries of the real QQ APK and run a targeted query.
5. [x] Update bilingual docs, validate builds/size and obtain Pro source review.
   No substantive blocker was found. Adopt the error-propagation, raw DEX
   mutation-test and tool-description refinements; validate the final release.
6. Publish to `teble:mcp`; Git and the completion report record the remote revision.

DEX byte budgets are resource guards, not a process-wide memory or CPU limit.
The source must remain unchanged during open; no full-APK copy is retained in
memory or temporary storage.
