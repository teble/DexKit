# Independent Planus / Core interoperability tests

This Cargo package uses the production `dexkit-rs` and `dexkit-sys` crates. It
does not depend on the MCP SDK or transport. An independent C++ FlatBuffers
producer constructs reference string queries against the same Core; Core is
linked from dexkit-sys, not rebuilt through a duplicate test bridge.

```sh
python3 tests/interop/planus/run.py
```

The runner uses the repository's pinned smali 2.5.2 dependency to assemble valid
fixtures, then runs Cargo with the checked-in lockfile. Build and fixture outputs
are ignored under this directory's `target/`; generated bindings stay in OUT_DIR.
Requires the normal repository Java/Gradle setup as well as Rust/CMake/C++20.

Coverage includes number and annotation union vectors, nested matchers, boolean
groups, empty candidate sets, positional wildcard parameters, native numeric
bits, field/code relationships, instance ownership, materialized pages, smali
limits and artifact delivery. Unicode cases exercise both directions through
the actual Core, with an independent C++ query producer. NUL and supplementary
characters match and decode; isolated surrogate metadata returns an explicit
encoding error, while escaped smali can preserve the original units.

The initial stock-Planus experiment established binary interoperability but
reproduced failures from treating MUTF-8 as UTF-8. Those diagnostic-only failures
have been replaced by successful normal-Unicode round trips through the actual
adapter. Neither the shared FBS layout nor the Android dependency graph changed.

The fixtures are deliberate valid inputs, not proof of a complete DEX verifier.
These tests do not claim performance bounds, arbitrary malformed-input safety,
all-platform execution, or full matcher coverage. MCP protocol tests live in
`mcp/tests`; Kotlin Unicode regression tests live with the JVM library.
