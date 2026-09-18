# Narrow index types: source review

[Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)

The first complete reply read `7cf47ce` -> `b3d2e5d` through GitHub. It checked
the new type header, compact indexes, producers, matchers, callers, cross-DEX
aggregation, diagnostics, components and build/contract changes. It found a
32-bit test argument that truncated before entering the checked function, and
identified the new method-table limit as a real compatibility restriction.
An actual 65537-entry DEX reproduced that restriction: the control accepted it,
while the fully narrow prototype aborted. The user explicitly chose to retain
32-bit method identities. That prototype is not the delivered candidate.

The complete follow-up read the fixed target
`c537b230c50bf08ac5a5e306d577ec0a6cb6c065`, including the `e537fb8`, `858beda` and
`3656323` fixes. Its 7m18s reply covered the type split, compact string/caller
indexes, DexItem declarations and construction, cross-DEX/Bean paths, matchers,
caller fill, diagnostics, component checks and contracts. It did not run tests.

## Findings and local disposition

- No confirmed new production blocker remained in the inspected increment.
  `LocalMethodId` is always u32. Method definitions, pending IDs, caller sources
  and field reverse identities preserve that width; the method-table rejection
  is gone. `InvokeOperandId` only narrows the local instruction operand.
- A method with ID 65536 can own a forward row containing operand 0; its caller
  identity stays 65536. The reverse cross-DEX direction also keeps resolved
  target identity in u32, independently of its local invoke operand.
- Invoke producers, noncompact vectors, compact storage, Hungarian arguments,
  held spans and diagnostic element sizes all use the separate operand type.
  Counts remain u32 or wider; order, duplicate witnesses and published views
  retain their original semantics and lifetime.
- The portable checked conversion is equivalent for the unsigned destination
  types actually used. Signed negatives are rejected before converting to
  `uintmax_t`; the upper bound is checked before the final conversion. It is not
  advertised as a general replacement for signed-destination `std::in_range`.
- The invalid 32-bit test now runs its size_t-wide case only on a wider host.
  Direct u64 narrowing, negative values and offset maximum plus one remain
  checked on both widths. A two-row prefix test rejects max plus one before
  allocating the edge payload.
- Remaining capacity limits are explicit: accumulated cache offsets/counts fit
  u32 and the class-definition table fits 65536 entries. The change preserves
  wide method identity, not arbitrary malformed-input acceptance. The component
  output `max_id:65535` describes its invoke boundary, not an engine method limit.
- The review suggested one nonblocking regression case: a local low invoke
  operand resolving to a remote definition with ID 65536. A generated two-DEX
  fixture adds this exact case, checking forward results, alias/definition
  getters, reverse callers and public class members before and after full cache.
  This passed the same-source control/candidate, all-OFF/NARROW-only,
  ASan/UBSan and compact/packed-field combination. The checker preserves the
  existing caller getter behavior: aggregation clears the local alias row;
  reverse metadata is fetched from the resolved definition row. An initial
  assertion wrongly expected the alias row to redirect; the control exposed
  that test mistake, which was corrected without changing production code.

The fully narrow prototype's 4-byte reverse-edge estimates and test outcomes
are not reused as final evidence. Actual final layout keeps caller and field
reverse records at eight bytes. Source review does not substitute for the final
JVM, Android compilation, sanitizer, oracle or timing records.
