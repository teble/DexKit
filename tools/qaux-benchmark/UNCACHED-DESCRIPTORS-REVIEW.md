# Uncached descriptors: source review

[Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)

The complete 8m45s reply read the fixed increment
`6544800dd1ab34bf77c2b59f1ae1eaf12ae7bf62` ->
`2be1a63f1ac9de56f6e5cd53123322388e598006` through GitHub. It inspected Bean types,
DexItem allocation/getter/lookup/cross-reference branches, CMake and both Gradle
connections, diagnostics, descriptor checks, the owning checker and native
contracts. It also read ordinary Find, Batch, GetByIds, nested serialization and
the existing raw descriptor parser. The review did not run tests or recompute
measurements. A retry label on the webpage did not prevent actual source access.

## Findings and local disposition

- No confirmed new dangling view, SSO relocation error, or changed duplicate
  representative was found in the inspected production paths. Method/field
  Beans own actual strings without a separate self-referential view. Find builds
  its `set<string_view>` after all result collection/relocation and consumes it
  while those Beans stay alive; serialized output copies bytes synchronously.
- This remains a conditional C++ type/ABI change. A view made from a temporary
  Bean dangles after that expression. Retain the Bean or an owning string, and
  do not assume views survive owning-string mutation/move. Existing docs cover
  this; no claim of transparent native compatibility is made.
- MemberValue/EnumValue/UsingFieldBean ownership does not make a complete native
  AnnotationBean or Batch wrapper safe after close. Annotation names, types,
  StringValue, ClassBean and batch keys can still borrow raw/query memory. The
  high-level APIs serialize while their owners live. Documentation preserves
  these original lifetime requirements; the new check covers owning member
  leaves and serialized results only.
- Raw lookup is unchanged code, but selecting it is still an algorithm change
  relative to cached-text lookup. The raw-dense control isolates it from removal
  of the member cache plus result ownership. A raw lookup miss need not call the
  descriptor getter, so it does not necessarily generate any strings.
- Macro guards remove the complete persistent method/field cache, ready arrays
  and descriptor publication locks. FAST_HITS does not fall into an absent cache.
  Existing query admission, raw identity publication and close restrictions stay.
- Zero descriptor census is only zero persistent bridge storage. `Built` counts
  construction calls, not every allocation/copy. Batch and existing result
  collection copy owning strings without incrementing that counter. These costs
  stay in timing and physical peak; the candidate does not silently add moves.
- Returned construction strings can retain capacity growth slack, whereas an
  old dense lvalue copy can use a different capacity. Subsequent Bean copies can
  change capacity again. Equal descriptor bytes are not equal allocation costs;
  no physical-memory explanation is inferred solely from character lengths.
- The checker calls strings shorter than 16 bytes SSO without directly observing
  inline storage. The review treats this as a non-blocking reporting issue.
  An additive checker linked the unchanged normal Core archive and observed
  actual host inline storage for the 14-byte method and 13-byte field. The
  1778-byte method and 30-byte field use heap storage. Their construction/copy
  capacities are 2815/1783 and 47/31 respectively. The added reads leave the
  complete ownership checks intact. Android evidence remains compilation/layout
  only, not executed SSO lifetime checks on four devices.

No production fix was required by this review. The finite comparison retains
all result construction, copy/move, serialization, destruction and close costs,
with old dense and vector as direct comparators and raw dense as attribution.
Tests and performance evidence are recorded separately from this source review.
