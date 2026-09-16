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
- [x] Verify same-API retained views, concurrent admission, native sessions,
      warmup exclusion, dense publication, bounds and intentional rebuilding.
- [x] Run normal/diagnostic/independent/sanitized Core oracles, QQ verification,
      required JVM and Android builds, and actual hot call-site inspection.
- [x] Review the fixed source increment with Pro and reconcile concrete issues.
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

The fixed comparison contains 82 sweeps / 984 fresh processes: a main phase and
one independent confirmation, with six balanced AB/BA pairs per sweep. Eighteen
native cases and three QQ cases compare the current pointer hybrid with vector.
Twelve relevant native cases and all QQ cases also compare old dense with vector;
four native cases and QQ p1/w4 compare no-promotion with vector. All four normal
artifacts were built from `4f7a82c` with the same Small14 options. Later test-only
and documentation refinements do not replace the frozen Core binaries.

Single-API-then-close is the one-calling-thread transition case. Four callers
perform four times the work and may mix sparse/dense generations in their first
stage; an outer barrier does not guarantee simultaneous query admission. Keep
the phase as actual competing calls, without subtracting conversion or rebuilding.
The threshold is a maintenance request, not a maximum live allocation budget.
