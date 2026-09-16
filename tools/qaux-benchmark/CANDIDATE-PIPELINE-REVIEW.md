# Ordinary candidate execution: review record

Historical review of the `9227210` / `82d6afb` implementation. The later
[ordinary find extraction](ORDINARY-FIND-EXTRACTION.md) removes QueryRun and its
exception recovery at the user's request. That change has separate local
validation; it was not part of the Pro review recorded below.

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

The user authorized implementation after the finite design review. The existing
Pro conversation was used for read-only source review through the selected
GitHub connector. Pro did not execute builds, tests or measurements.

| Review | Actual source scope | Findings and local response |
| --- | --- | --- |
| Design, 8m37s | Fixed `0dc49fff2042cd33b91fdd97cc3813d7e50098e9`; ordinary and Batch find paths, matchers, query execution, scheduler and thread pool | Separate candidate preparation from consumption in the interface, with successful validation remaining in its producer worker by default. Preserve complete root truth, original ID ranges, ordered output, bounded retained arrays and explicit query cleanup. Keep Batch and conservative admission unchanged. |
| Implementation, 14m25s | Fixed `0dc49fff2042cd33b91fdd97cc3813d7e50098e9 -> 9227210eaaa7932020b5653707e6d1274cb6e5df`; new runtime/candidate/coordinator files, method/class integration, CMake/Gradle wiring, new checks, and relevant existing executor/scheduler/thread-pool code | No confirmed new false negative, reordering or premature input release under the contract that accepted work eventually executes or releases its captures. It identified omitted keyword-plane headers and last-string IDs in the array estimate, and a failure-test gap because simulated executor destructors join. Both were addressed locally. |

The final engine is `82d6afb`. This follow-up was validated locally; it was not
part of the Pro source review above:

- Add the raw-keyword upper bound for plane headers, last-string IDs and the
  single output-group entry. The two-million-keyword/one-entity counterexample
  exceeds the default array reservation on the measured 64-bit build. Checked
  multiplication/addition rejects size overflow. Temporary associative
  containers and trie-hit buffers are explicitly outside this controlled-array
  budget; it is not a limit on total candidate work or process memory.
- Run the preparation/validation failure gates with the real shared scheduler
  held outside QueryRun. Executor destruction only detaches; it cannot make
  the tests pass by joining the pool. Also verify actual slice counts and
  boundaries in the private production-consumer integration check.
- Fix a real Android build failure discovered locally: Android uses
  `-fno-exceptions`. Conditional exception handling preserves that policy;
  invalid internal state aborts in that build. A no-exceptions host check and
  all four Android ABI builds pass, alongside exception-enabled failure tests.

The inherited scheduler allocation-failure boundary remains: failures between
in-flight bookkeeping and successful dispatch can leave internal state that
QueryRun cannot repair. Controlled Submit rejection tests are not a guarantee
of arbitrary scheduler-internal OOM recovery. No scheduler policy was changed.

FIFO coordination waits for the current source's validation before refilling
its bounded preparation window. Candidate directory construction and Bean
moves are also part of the measured change. The final report separates the
combined refactor from optional successful re-slicing, and does not attribute
all differences to parallelism or claim broader admission benefits.
