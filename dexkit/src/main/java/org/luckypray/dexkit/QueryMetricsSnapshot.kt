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
            return QueryMetricsSnapshot(
                submittedTasks = values[0],
                dispatchedTasks = values[1],
                baseDispatchedTasks = values[2],
                bonusDispatchedTasks = values[3],
                completedTasks = values[4],
                maxInFlight = values[5],
                maxQueryShareCount = values[6],
                firstDispatchDelayNs = values[7],
            )
        }
    }
}
