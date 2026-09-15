# String AND tree versus a flat root matcher

This follow-up answers whether removing allOf makes the broad nested workload faster. The engine remains the corrected `cc9f893`; all five workload executables link the same immutable control or inverse Core archives. No production matcher or timed artifact from the main experiment was changed.

## Three query forms

```text
nested:  Contains(Needle) AND allOf(Contains(LongPrefix/), Contains(Needle))
flat:    usingStrings(Needle, LongPrefix/, Needle), all Contains / case-sensitive
root:    usingStrings(Needle), Contains / case-sensitive
```

The flat form preserves the complete AND predicate, including the negative query (AbsentEveryPool AND LongPrefix/ AND Needle). The root-only form removes a requirement. This equivalence is limited to these same-mode string atoms; it is not a generic rule for merging every matcher type or colliding match modes.

The small independent raw-row oracle returns four positive methods for both nested and flat, and 45 for root-only. Complete ordered FlatBuffer bytes agree for nested/flat on both engines. On the broad fixture, all forms return the same 4500 positive methods and zero negative methods, so timing root-only there does not demonstrate general equivalence.

## Measured comparison

The fixture has one DEX, 4500 methods and sixteen LONG references per method. Every fresh process performs sixteen positive/negative pairs with four configured engine workers. Each comparison has six balanced randomized process pairs and an independently seeded confirmation: six sweeps / 72 samples total. Negative percentages favor the second form; intervals are exploratory paired bootstrap 95% intervals. Absolute numbers are marginal medians.

| Comparison | Main lifecycle change and interval | Confirmation lifecycle change and interval | Confirmation before -> after (ms) |
|---|---:|---:|---:|
| Nine-option control: nested -> equivalent flat | -43.08% [-45.39, -41.72] | -41.98% [-45.21, -39.32] | 1790.507 -> 1051.467 |
| Inverse: nested -> equivalent flat | -98.38% [-98.39, -98.37] | -98.38% [-98.39, -98.36] | 2237.505 -> 36.388 |
| Inverse: nested -> root only | -98.40% [-98.42, -98.40] | -98.39% [-98.41, -98.36] | 2242.675 -> 35.958 |

| Comparison / leg | Main change and interval | Confirmation change and interval | Confirmation before -> after (ms) |
|---|---:|---:|---:|
| Control flatten: positive APIs | -59.13% [-60.67, -58.13] | -58.26% [-60.39, -56.78] | 1364.549 -> 577.359 |
| Control flatten: negative APIs | +8.14% [+3.45, +11.69] | +10.50% [+2.62, +18.83] | 416.167 -> 473.328 |
| Inverse flatten: positive APIs | -98.43% [-98.45, -98.43] | -98.44% [-98.44, -98.42] | 2235.959 -> 35.083 |
| Inverse flatten: negative APIs | -27.50% [-34.11, -22.46] | -24.64% [-28.76, -19.20] | 0.964 -> 0.707 |
| Inverse remove: positive APIs | -98.45% [-98.46, -98.44] | -98.44% [-98.45, -98.40] | 2241.106 -> 34.922 |
| Inverse remove: negative APIs | -64.29% [-67.04, -60.67] | -69.07% [-69.58, -56.69] | 0.994 -> 0.323 |

## Interpretation

The slowdown is not the boolean allOf check alone, or simply that it runs after other conditions. The current inverse planner only compiles root usingStrings. The nested children retain per-method AC scans; a broad one-DEX root also leaves that work in a single DEX task. Moving the runtime allOf call earlier would not remove those scans or restore range parallelism.

Flattening puts all required strings into the same root plan: one AC pass over referenced distinct strings, followed by keyword bitmap intersections, replaces the nested per-method scans. The inverse lifecycle falls from about 2.238 seconds to 36.4 milliseconds without dropping conditions. The old control also improves (about 42%) because one combined AC matcher replaces repeated nested scans, despite having no inverse planner.

Early rejection still matters. In the old nested negative query, the absent root avoids the children; a combined AC matcher still reports the other words while checking absence. The control negative leg therefore regresses by 10.50% in confirmation, even though its complete lifecycle improves. This is a counterexample to treating removal/reordering of allOf as universally beneficial.

For this positive predicate, the Kotlin equivalent is:

```kotlin
usingStrings("Needle", "LongPrefix/")
```

The vararg API uses Contains and is case-sensitive by default. anyOf represents OR and cannot generally be replaced by the same AND-valued usingStrings list. A future planner would need unions for OR and intersections for AND, while preserving the semantics of the individual matcher nodes; this follow-up did not add that engine transformation.

[Raw summary](evidence/string-inverse/shapes/summary.json), [validation](evidence/string-inverse/shapes/validation.json), [archive manifest](evidence/string-inverse/shapes/manifest.json). The main QQ and native comparison remains in [STRING-INVERSE-RESULTS.md](STRING-INVERSE-RESULTS.md).
