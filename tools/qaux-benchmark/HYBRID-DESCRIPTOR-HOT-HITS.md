# Hybrid descriptor fast-entry follow-up

The first hybrid batch retained the storage experiment conditionally: QQ used
less physical peak memory, but dense output and repeated descriptor lookup
regressed against the old dense cache. The normal macOS arm64 getter built its
factory closure and a 32-byte stack frame even when the dense slot was present.
That is an observed cost, not proof that it explains the complete regression.

This follow-up moves factory materialization into a noinline Cold entry when
both HYBRID_DESCRIPTORS and DESCRIPTOR_FAST_HITS are enabled. A shared TryGet
performs the same bound check and two acquire loads before reading the immutable
string. Sparse misses and unfilled dense slots still recheck under the existing
mutex. A failed outer probe counts nothing; the eventual hit or slow access
counts once. The generic path remains available with FAST_HITS disabled.

Keep ownership, deque payload construction, dense publication, cache layout and
the conversion byte rule unchanged. No new storage flag or default change is
part of this increment. The extra failed probe on the cold path is a possible
cost and must be measured too.

- [x] Implement the shared probe and separate method/field Cold entry.
- [x] Validate misses, empty/SSO/long hits, bounds, accounting, publication and
      concurrent construction with normal, diagnostic and sanitizer builds.
- [x] Check actual normal getter assembly; verify FAST_HITS disabled and the
      sparse-only control, JVM tests and four Android ABIs.
- [ ] Push the stable increment to the authorized fork branch and obtain a
      focused review in the existing Pro conversation.
- [ ] Freeze a finite old-hybrid/new-hybrid comparison with Small14 unchanged.
      Include full SSO 2/16 repeats, wide/prefix/hot lookup, one/four calling
      threads, QQ p1/w4, p11/w4 and p1/w1, sparse output, one member per domain,
      conversion boundaries with immediate close, and long output.
- [ ] Run six balanced pairs in a main phase and one independent confirmation;
      preserve adverse results and report first, warm, close, lifecycle and
      physical peak. Use the old dense cache as a residual-cost reference for
      the cases that motivated this change. Decide whether to retain this
      increment separately from the storage design.

The baseline is the immutable hybrid artifact from `bf9cee5`, recorded in the
preceding [results](HYBRID-DESCRIPTORS-RESULTS.md). New evidence has its own output
directory; the completed hybrid batch is not rebuilt or overwritten.

## Validation at the fixed source

Source `56c680c68f44987979c96f01ccd461a546a1e5cd` passed 184 native driver
commands: 40 normal, 56 diagnostic, 32 isolated and 56 ASan/UBSan. These include
28 expected bound-rejection aborts in each of the normal/diagnostic/sanitizer
groups. FAST_HITS is OFF in both isolated hybrid and isolated sparse builds.
There were 42 normal workload smokes, two normal QQ full-oracle checks, one
diagnostic QQ check and one normal QQ check with late field readers/writers.
The JVM suite passed 71 tests with zero skips; the AAR contains arm64-v8a,
armeabi-v7a, x86 and x86_64. Android evidence is build validation, not runtime
performance. Host sanitizer execution disabled leak detection.

Sixteen before/after diagnostic cases reconcile all 4352 pre-close domain tables,
retained requested capacities and completed calls/hits/records with the outer
descriptor diagnostics. Conversion timing and racing lock-free hit counts are
excluded from pairwise equality. The host fixed cache is still 6680 bytes and
the observed 448/449 conversion boundary is unchanged. These diagnostic deque
requests do not establish a physical allocation breakdown for normal builds.

Actual normal macOS arm64 assembly has 27 instructions per new getter, versus
38 before and 13 for the old dense reference, including all branches. Normal
successful hits do not touch the stack; misses tail-call the Cold entry. Only
the invalid-bound abort branch creates a frame. Instruction counts do not imply
an equivalent cycle reduction or explain the complete previous regression.

The bounded matrix has 18 before/after cases and eight old-dense/after reference
cases, each with six balanced pairs in a main phase and one confirmation: 52
sweeps and 624 fresh processes. The residual references are full SSO 2/16,
wide/prefix/hot lookup and the three QQ cases. The source review was submitted
in the existing Pro conversation and remains pending at this validation record.
Formal timings have not yet completed; storage remains experimental and OFF.
