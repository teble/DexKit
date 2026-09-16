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
