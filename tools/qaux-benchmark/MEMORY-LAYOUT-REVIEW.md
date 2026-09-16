# Memory layout design review

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

Pro read fixed source `174b69ad699702ee6e38da0b998192fca470d030`, including
the completed caller report, relevant historical descriptor results, descriptor
initialization/getters/cold construction, Bean/lookup/matcher/serialization
consumers, diagnostics and both name maps. It also inspected the bundled phmap
node/flat policies, rehash, destruction and allocation-layout helper. It did not
run tests or timings. The complete final response was read and checked locally.

## Descriptor direction

Start with one independently measured node-hash prototype. Each DexItem has
32 shards; each shard owns one mutex and separate method/field node maps.
All lookup and insertion operations use the corresponding shard lock. Copy a
view to the completed string while locked, then permit immutable character reads
after unlock. Never erase or modify published entries, move the cache, or use
the map's iteration order for results.

The bundled node policy allocates each key/value pair separately and transfers
only its pointer during rehash. This preserves the string object and its SSO
body, unlike direct string values in a flat table. It does not permit concurrent
unlocked map reads during rehash. Full coverage still pays one allocation and
deletion per node plus bucket/control bytes, so the earlier SSO regression remains
a required counterexample rather than a promised speed result for this prototype.

Keep descriptor formatting, append order, lvalue copy into the final string,
local-ID caching and existing cross-DEX representative selection. The cold
builder must not reenter the cache lock. Remove the full-ID optional arrays and
ready-byte arrays for this option; retain logical ID bounds and reserve no table
capacity from total IDs. Standalone enablement must remain safe without the
structural-descriptor option. Keep lookup and cold construction separate enough
to avoid enlarging the hot path unnecessarily.

Flat hash plus stable string blocks and fixed ID pages remain alternatives,
not parallel implementations required before the first result. A stable-block
comparison would need evidence that node allocation/destruction dominates while
hash/lock costs remain acceptable. Fixed pages first need actual page occupancy.

## Census and checks

The bundled `HashtableDebugAccess<Map>::AllocatedByteSize` calculates requested
layout bytes including control/slot alignment and policy-reported node objects.
For name maps, string-view keys borrow raw bytes. For descriptor node maps, add
only external non-SSO character capacity after that helper; do not count the
string object twice. Fixed maps/locks, growth and bucket overlap are separate;
two full helper totals double-count nodes during rehash.

Record ready-ID occupancy at page sizes 64, 256 and 1024, split by DEX and
method/field. Getter access counts and shard distribution can establish whether
sparse materialization hides many hot reads. Keep added statistics out of normal
timing builds. Preserve cold concurrent same-ID, same-shard and different-shard
tests, forced actual growth, and an independent reader of previously retained
SSO/long/empty views. Four search workers alone do not exercise four concurrent
calling threads in descriptor lookup.

Use QQ one/eleven passes, concentrated/scattered/one-shard IDs, 120k SSO full
coverage with 2/16 repetitions, long signatures and wide/hot/prefix misses.
Retain ordered identity/FlatBuffer oracles, complete lifecycle, first/warm API,
close and physical peak. Classify results as retained, conditional or stopped
according to evidence, without inventing a universal percentage threshold.

## Other candidates

Pro retained the order of packed cross references, per-row packed field uses and
implicit class-field ranges. These do not depend on descriptor hash succeeding.
The subsequent packed-cross implementation and new census are local follow-up
work, not code inspected in this design review. Pointer/offset/raw-proto changes
and forward invocation capacity remain later candidates; already published
spans must not be invalidated by shrink_to_fit.

## Cross-reference and node source review

Pro subsequently read the complete fixed increment
`174b69ad699702ee6e38da0b998192fca470d030` ->
`c9af4af00d2da3fabed4916d75756d6f7dec94d4` ->
`5ca19ac8c95d0c75635c43945e25a8a4b6a53add`, including the new types,
producer/consumer and macro changes, diagnostics, four tests/workloads and the
bundled phmap implementation. Its complete final answer was read. It did not
run tests or measurements and found no confirmed new production correctness
blocker within the existing publication, immutable-view and quiescent-close
contracts.

The value encoding preserves empty versus (0,0), all u16/u32 pairs and carry
across the low word. Ordinary 64-bit accesses on 32-bit targets still depend on
the existing publication protocol. Node views are formed from completed nodes
before the lock releases; no growing table is accessed without its owning lock,
and the cold factories do not reenter the cache.

The review identified two reporting limitations that were checked locally:

- Diagnostic counter alignment can change total object padding. The original
  subtraction is wrong by 132 bytes on armeabi-v7a; `05e5a41` computes an
  equivalent normal layout. Separate normal/diagnostic size probes agree on
  macOS arm64 and all four Android ABIs. This change is diagnostic-only and the
  earlier macOS totals remain valid.
- The first concurrent workload ended its lifecycle before destroying small
  harness containers. `62687fb` moves the workload into an inner scope and takes
  the lifecycle sample after their destruction. Warm time is a group of four
  APIs per calling thread, not single-lookup latency. Four callers do four times
  the work of one, all sharing hot keys; first time includes barrier arrival.

The dense capacity census historically omits the two vector owners and two
ready-array unique_ptr owners, while the node census includes its fixed cache
object. The final symmetric descriptor accounting must add 64 bytes per DEX
(2624 bytes for this host/QQ input) to the raw dense census. Keep this adjustment
visible rather than changing the old raw records.

No new cache redesign was recommended. A cached-text hit still calls GetBean,
which requests the same descriptor again; this behavior predates the node
experiment, but now pays another synchronized hash lookup. Miss insertion also
does find followed by try_emplace, and full coverage retains per-node deletion.
These are possible explanations for measured adverse behavior, not separately
measured causes or reasons to mix another optimization into the comparison.

## Field-use and correction source review

Pro read the complete fixed increment `5ca19ac` -> `62687fb` -> `05e5a41`,
including field encoding, its producers/consumers, Hungarian and compact-field
types, both Gradle paths, diagnostics and concurrent workload/sweep. The complete
final response was read. It ran no code and found no new production correctness
blocker in that increment.

The zero token is a valid field-0 Put, not an empty sentinel. Current instructions
carry local 16-bit references; the pre-shift range check does not limit public
field IDs or cross-DEX representatives. Decoding remains at the old reverse
builder, getter and matcher boundaries. Hungarian still distinguishes duplicate
positions and uses the original evaluation order, with a smaller copied element.

The normal-layout reconstruction fixes the diagnostic padding issue. Future
normal-cache members must also be reflected in the equivalent diagnostic layout;
the existing five-ABI probes check that correspondence. The host-only 64-byte
dense owner adjustment must not be reused blindly on other ABIs. Field payload
halving leaves row directories and buffer counts unchanged.

Pro correctly identified packed-field plus COMPACT_FIELDS execution as pending
at submission time. Local follow-up subsequently built that combination and
passed 28 driver commands, including held row views and independent field byte
oracles. The normal, isolated/trace and ASan/UBSan validation groups also passed;
71 JVM tests and all four Android ABI builds passed. These are local execution
results, separate from the source review.

No extra cache or relation redesign was requested. Continue the fixed independent
and direct-combination measurements, including token decoding, vector copies,
allocation sizes and complete destruction; report the main and confirmation
results without equating layout-byte savings with physical peak savings.
