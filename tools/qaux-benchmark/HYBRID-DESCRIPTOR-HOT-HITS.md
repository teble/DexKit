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
- [ ] Validate misses, empty/SSO/long hits, bounds, accounting, publication and
      concurrent construction with normal, diagnostic and sanitizer builds.
- [ ] Check actual normal getter assembly; verify FAST_HITS disabled and the
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
