# Independent vector descriptor experiment

The user authorized validating an independent `vector<string>` dense backend.
The completed Pro design review at `3118b4e` recommends discarding sparse values
at a safe API boundary, then lazily rebuilding the dense cache. This experiment
does not move or reference old strings. Existing storage variants remain fixed
comparators and all new options default OFF.

Use one fixed vector and atomic ready array per DEX member domain; method and
field domains convert independently. Keep 32 cold locks. Sparse payload remains
stable within a borrowing session. Normal-build allocator bookkeeping records
the reclaimable deque block/directory requests needed for the new byte rule;
external character buffers do not count as permanent structural savings.

Misses only request conversion. The next top-level admission stops new entries,
drains active queries and excludes warmup before freeing old sparse storage and
constructing the empty dense vector/ready array. The triggering query completes
and serializes its existing Bean views before any reclamation. Closing without
another query does not perform a pending conversion.

The opt-in native contract requires a descriptor borrowing session for direct
Bean access. Ordinary APIs install one as part of their existing query guard;
worker tasks inherit its context. Borrowed Beans/views and pending worker use
must end before that session ends. A top-level API must not be entered again
for the same DexKit while holding a native session. Enforce missing/reentrant
sessions, and avoid per-descriptor read locks or ownership reference counts.

- [x] Implement the independent backend, normal allocation accounting, scoped
      native borrowing and admission maintenance, with a no-promotion control.
- [ ] Verify same-API retained views, concurrent admission, native sessions,
      warmup exclusion, dense publication, bounds and intentional rebuilding.
- [ ] Run normal/diagnostic/independent/sanitized Core oracles, QQ verification,
      required JVM and Android builds, and actual hot call-site inspection.
- [ ] Review the fixed source increment with Pro and reconcile concrete issues.
- [ ] Freeze a finite comparison against old dense, current pointer hybrid and
      the new no-promotion control, with a main phase and one confirmation.
- [ ] Complete measurements, retain adverse samples, archive evidence and report
      where conversion/rebuilding costs outweigh any steady dense improvement.

The comparison must include sparse QQ, full SSO one call then close/two/sixteen
passes, long output, wide/prefix/hot lookups, one/four calling threads, low unique
coverage with many hits and changed working sets after conversion. Report
reclamation/allocation/waiting, generation rebuilds, warm work, close, complete
lifecycle and physical peak separately. Old same-address/one-build-for-life
checks become session/generation checks only for the new experimental backend.
