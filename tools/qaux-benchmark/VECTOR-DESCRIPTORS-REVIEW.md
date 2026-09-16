# Independent vector source review and verification

[Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)

The complete 10m12s reply reviewed `3118b4e -> 4f7a82c`. Pro read the two new
headers, both task submission templates, vector checkers and transition workload;
it checked the admission/warmup, DexItem consumers, diagnostics, macro and native
contract changes. It did not execute code or measure performance. No confirmed
early reclamation, same-slot race or warmup/conversion overlap was found for
callers obeying the new native lifetime contract. This is a source review, not
proof of performance or transparent native compatibility.

## Findings and local disposition

- A captured context carries only an owner address. It neither owns an admission
  nor detects expired reuse. The contract now explicitly covers expired contexts,
  descriptor-reading task destructors and conflicting multi-bridge wait orders.
  Missing/current-thread reentrant sessions abort; arbitrary lifetime violations
  are not all detectable by this owner check.
- A retained native session can also block an independent API on another thread.
  During the full relation oracle, the existing sequence 5 held an admission and
  joined a thread performing independent forward APIs. The child needed descriptor
  maintenance and waited for that admission, while the parent waited for the child.
  A process sample confirmed both waits; the failed test/log was retained. The
  test now releases completed reverse preparation before launching/joining that
  independent API when vector storage is enabled. Its original ordered result
  oracle and all seven warmup sequences remain checked. Native documentation
  forbids this wait cycle; joined low-level workers share the existing context.
- Added actual scheduler checks for both ordinary and candidate task submission:
  a worker returns a borrowed MethodBean, the parent retains and later serializes
  it under its session, and a second API must wait for that serialization before
  conversion. These passed in the combined, isolated FASTOFF and ASan/UBSan builds.
- The old `--dense` symbol fixture check holds one session across its 120k member
  fill. Under vector storage it verifies a retained sparse generation that can be
  pending, not the dense backend by itself. The second API in CheckBasic and the
  post-conversion component checks verify the real dense backend separately.
- `entrant_wait_ns` only accumulates waits for active operations to drain after
  a maintenance request. It excludes other admission/warmup/inflight waits and
  can overlap between waiting threads. Do not add it to maintenance wall time or
  describe it as total latency. Complete API/lifecycle timing retains every wait.
- Four callers can enter their first APIs at different times, so the first stage
  can mix generations. Added diagnostic first-stage snapshots without changing
  the normal workload. Preprocessed old/new source hashes are identical in all
  four normal builds. The observed full-w4 check ended its first stage with 60k
  sparse records and pending conversion; this is not guaranteed for every run.
  The changed-set check confirmed 30k original IDs crossed the host byte rule,
  then were discarded; only the disjoint 30k IDs were built in the dense generation.

## Completed verification before performance measurement

- 219 native driver commands passed: normal 64, diagnostic 78, isolated 36,
  ASan/UBSan 41. They cover component limits, ready/empty values, published views,
  deliberate release aborts, descriptor/overload bytes, relation ordering and
  multiplicity, invoke fixtures, field fixtures and concurrent public APIs.
- Four additive worker-check commands and six first-stage diagnostic checks passed.
  Revised test executables link the original frozen Core archives; compilation
  records include commands, test source hashes and unchanged archive hashes.
- All 72 normal workload smoke processes agreed on counts/checksums; the new
  transition cases additionally match independently calculated ID sums/counts.
- Eight normal QQ full-oracle runs passed, across all four variants with one/four
  query workers. Two diagnostic QQ runs and two full field-tail oracles also passed.
  QQ diagnostic records showed no conversion in either new storage mode.
- 36 native diagnostic runs reconcile calls, hits, generation builds and resident
  records. Converted domains have zero old hash/deque/character allocations.
  One-call/close skips conversion; repeated calls rebuild only requested values.
- Five ABI layout probes agree with the normal-object size mirror: 7304 bytes on
  desktop, 6536 on Android arm64/x86_64, 2756 on Android armv7/x86. Diagnostics are
  excluded from those values. The host threshold probe requests conversion at
  28681 of 60000 sequential IDs: structural bytes 1500928 versus dense base 1500000.
- Required Gradle Core/JAR/JVM/Android tasks passed: 71 JVM tests, zero skipped,
  and an AAR containing all four Android ABIs. Documentation dependency install
  and final VuePress build passed. Sanitizer leak detection was disabled; no
  Android device performance measurements were performed.
- Actual normal getters have one ready acquire and direct 24-byte string-object
  indexing, replacing the pointer hybrid's two acquires/string pointer lookup.
  Public Bean scope checks still cost instructions. Static instruction counts do
  not establish timing gains.

Development also found that the first reentry check used DEXKIT_CHECK, which is
empty under Release. The frozen implementation uses an unconditional abort;
both missing-session and reentry rejection were rerun successfully. Neither this
development failure nor the retained relation-test wait is a discarded timing
sample. Performance comparisons start only after these checks finish.
