package org.luckypray.dexkit

import org.luckypray.dexkit.annotations.DexKitExperimentalApi

@DexKitExperimentalApi
enum class QueryMetricsKind(internal val nativeValue: Long) {
    FIND_CLASS(0),
    FIND_METHOD(1),
    FIND_FIELD(2),
    BATCH_FIND_CLASS_USING_STRINGS(3),
    BATCH_FIND_METHOD_USING_STRINGS(4),
    ;

    internal companion object {
        fun fromNative(value: Long): QueryMetricsKind {
            return values().firstOrNull { it.nativeValue == value }
                ?: error("Unknown query metrics kind: $value")
        }
    }
}
