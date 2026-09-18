# Performance Optimization

In DexKit, various queries may achieve the same functionality, but the difference in performance can
be significant, varying by several tens of times. This section will introduce some techniques for
performance optimization.

At the native layer, DexKit maintains lists of classes, methods, and fields in the Dex file. How
does DexKit scan these lists in several APIs? The traversal order of `findClass`, `findMethod`, and
`findField` is based on the respective lists' sequential order. Then, each condition is matched one
by one.

## declaredClass condition is too heavy

Some users may use the `declaredClass` condition to write queries like the following when using:

```kotlin
private fun badCode(bridge: DexKitBridge) {
    bridge.findMethod {
        matcher {
            declaredClass {
                usingStrings("getUid", "", "_event")
            }
            modifiers = Modifier.PUBLIC or Modifier.STATIC
            returnType = "long"
            addInvoke {
                name = "parseLong"
            }
            addInvoke {
                name = "toString"
            }
        }
    }.single().let {
        println(it)
    }
}
```

This search takes `4310ms`.

At first glance, this query seems fine, but in reality, its performance is very poor. Why? As
mentioned earlier, the `findMethod` API traverses all methods and then matches each condition one by
one. However, there is a many-to-one relationship between methods and classes, meaning a class may
contain multiple methods, but a method can only belong to one class. Therefore, during the process
of traversing all methods, each method will be matched once with the `declaredClass` condition,
leading to performance waste.

So, let's change our approach. By first searching for `declaredClass` and then using chain calls, we
can search for methods within the classes that meet the criteria. Won't this help avoid the issue?

```kotlin
private fun goodCode(bridge: DexKitBridge) {
    bridge.findClass {
        matcher {
            usingStrings("getUid", "", "_event")
        }
    }.findMethod {
        matcher {
            modifiers = Modifier.PUBLIC or Modifier.STATIC
            returnType = "long"
            addInvoke {
                name = "parseLong"
            }
            addInvoke {
                name = "toString"
            }
        }
    }.single().let {
        println(it)
    }
}
```

This search takes `77ms`, showing a performance improvement by several tens of times.

When using `findMethod` or `findField`, the `declaredClass` condition should be avoided as much as
possible.

## Native descriptor storage experiment

This experiment branch adds `DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS`, default OFF.
Gradle builds can enable it with `-PexperimentVectorDescriptors=ON`. It is mutually
exclusive with the node, sparse and hybrid descriptor storage experiments.
`DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS_NO_PROMOTION` (Gradle:
`-PexperimentVectorDescriptorsNoPromotion=ON`) keeps the same new sparse path and
borrowing contract but disables conversion, for a comparison control.

A DEX method or field domain starts sparse. Once its reclaimable structural
allocation reaches the cost of the dense string/ready arrays, the next top-level
operation drains admitted operations and excludes cache warmup before discarding
all old strings. The dense vector starts empty and rebuilds requested values
lazily. A triggering operation keeps its old views through serialization; closing
without another operation skips pending conversion. This is an experiment, with
no performance or allocation-failure recovery guarantee.

When this option is enabled, direct C++ access to Beans containing method/field
descriptors requires `auto borrow = bridge.BorrowDescriptors(required_flags)`.
Keep this nonmovable session on its creating thread until all returned Beans,
their descriptor views and any tasks using them are finished. Copy needed bytes
into owning strings before ending the session. `required_flags` performs required
cache warmup before admission. Do not call a top-level API on the same bridge while
holding the session; use the low-level `DexItem` accessors within it. Missing and
same-bridge reentrant sessions abort, including Release builds.
Do not hold a session while waiting for another thread to finish an independently
admitted top-level API on that bridge: that API can need maintenance, which waits
for the session. The thread-local reentry check cannot detect this cross-thread
wait cycle. Finish preparation and release the session before starting/joining
independent API calls; workers within a live session must share its context and
use low-level accessors.

For native worker tasks, capture `DescriptorBorrowScope::Capture()` inside the
session and construct `DescriptorBorrowScope scope(context)` on each worker.
This context borrows only the current bridge's session: it does not own the bridge,
extend the session, or inherit outer sessions for other bridges. Join all such
workers before ending the owning session. Ordinary query APIs and their internal
workers establish these scopes themselves. Their returned FlatBuffers own their
serialized descriptor bytes; Java/Kotlin query APIs and existing examples keep
their current lifetime behavior. Closing/mutating a bridge concurrently remains
outside the supported native lifecycle.
The owner check does not detect an expired captured context. Do not reuse it
after its owning session ends, and include any descriptor-reading task destructors
in the worker lifetime. Avoid conflicting acquisition/wait orders when using
sessions for more than one bridge.

## Uncached member descriptors

`DEXKIT_EXPERIMENT_UNCACHED_DESCRIPTORS` defaults OFF. Gradle uses
`-PexperimentUncachedDescriptors=ON`; also enable `experimentStructuralDescriptors`
and `experimentRawDescriptorLookup`. It is mutually exclusive with the other
descriptor storage experiments, including vector. The fast-hit option has no
effect on uncached descriptor access.

In this experiment, `MethodBean::dex_descriptor` and `FieldBean::dex_descriptor`
are owning `std::string` values. Copying a Bean copies its descriptor; moving the
Bean transfers ownership. A `string_view` derived from a Bean borrows that Bean's
string and must not outlive it or survive an invalidating move/mutation. Do not
store a view obtained from a temporary Bean. All native users and the library
must agree on this option because it changes C++ types and ABI.

Every requested method/field descriptor is generated again. The bridge retains
no descriptor array/hash cache, ready bytes or descriptor publication locks, and
does not use the vector experiment's borrowing sessions. Existing bridge warmup,
query admission and native close/mutation restrictions still apply. Class and
other DEX-backed views retain their existing lifetime requirements; this option
does not make all native metadata own its underlying data.

Result assembly and textual cross-DEX deduplication use live owning Bean strings.
Serialized FlatBuffers and Java/Kotlin results retain their existing bytes and
API behavior. Repeated output may spend more time generating descriptors, and
simultaneous owning results can increase temporary allocations. Performance must
be evaluated with the complete workload; removing persistent storage is not a
guarantee of lower process peak memory.

## Narrow native index types

`DEXKIT_EXPERIMENT_NARROW_TYPES` defaults OFF; Gradle accepts
`-PexperimentNarrowTypes=ON`. It uses 16-bit stored local method IDs and class
definition indexes, four-byte caller/field-reader/field-writer references, and
32-bit offsets in compact relation indexes. Vector growth and row ordering are
unchanged. String IDs and row lengths remain 32-bit; jumbo string references and
rows longer than 65535 entries are preserved. Public Bean/schema IDs stay wide.

This opt-in representation requires at most 65536 method IDs per DEX and checks
that bound during initialization. ID 65535 is valid, rather than an empty marker.
Unrepresentable accumulated offsets/counts and narrowing conversions terminate
through an always-enabled check; they never wrap silently. These are experiment
limits, not a claim that every input accepted by the original Reader fits the
narrow representation. Keep the switch OFF when the wider input domain is needed.
All native consumers must use matching definitions because internal C++ layout
and types differ. The Java/Kotlin API and serialized result format are unchanged.
