package org.luckypray.dexkit

import org.luckypray.dexkit.annotations.DexKitExperimentalApi

@DexKitExperimentalApi
enum class QueryMetricsPriority(internal val nativeValue: Long) {
    NORMAL(0),
    LATENCY_SENSITIVE(1),
    ;

    internal companion object {
        fun fromNative(value: Long): QueryMetricsPriority {
            return values().firstOrNull { it.nativeValue == value }
                ?: error("Unknown query metrics priority: $value")
        }
    }
}
