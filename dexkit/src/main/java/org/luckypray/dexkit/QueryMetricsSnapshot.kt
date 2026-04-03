package org.luckypray.dexkit

import org.luckypray.dexkit.annotations.DexKitExperimentalApi

@DexKitExperimentalApi
data class QueryMetricsSnapshot(
    val submittedTasks: Long,
    val dispatchedTasks: Long,
    val baseDispatchedTasks: Long,
    val bonusDispatchedTasks: Long,
    val completedTasks: Long,
    val maxInFlight: Long,
    val maxQueryShareCount: Long,
    val firstDispatchDelayNs: Long,
) {
    internal companion object {
        const val NATIVE_SIZE = 8

        fun fromNative(values: LongArray): QueryMetricsSnapshot {
            require(values.size == NATIVE_SIZE) {
                "Unexpected query metrics size: ${values.size}"
            }
            return fromNative(values, 0)
        }

        fun fromNative(values: LongArray, offset: Int): QueryMetricsSnapshot {
            require(offset >= 0 && values.size >= offset + NATIVE_SIZE) {
                "Unexpected query metrics slice: size=${values.size}, offset=$offset"
            }
            return QueryMetricsSnapshot(
                submittedTasks = values[offset],
                dispatchedTasks = values[offset + 1],
                baseDispatchedTasks = values[offset + 2],
                bonusDispatchedTasks = values[offset + 3],
                completedTasks = values[offset + 4],
                maxInFlight = values[offset + 5],
                maxQueryShareCount = values[offset + 6],
                firstDispatchDelayNs = values[offset + 7],
            )
        }
    }
}
