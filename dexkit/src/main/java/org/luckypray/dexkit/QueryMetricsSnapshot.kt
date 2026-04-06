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
    val firstBonusDispatchDelayNs: Long,
    val firstTaskStartDelayNs: Long,
    val lastTaskFinishDelayNs: Long,
    val taskRuntimeTotalNs: Long,
    val taskRuntimeMaxNs: Long,
    val preprocessCompletedNs: Long,
    val submissionCompletedNs: Long,
    val workersCompletedNs: Long,
    val completedNs: Long,
) {
    internal companion object {
        const val NATIVE_SIZE = 17

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
                firstBonusDispatchDelayNs = values[offset + 8],
                firstTaskStartDelayNs = values[offset + 9],
                lastTaskFinishDelayNs = values[offset + 10],
                taskRuntimeTotalNs = values[offset + 11],
                taskRuntimeMaxNs = values[offset + 12],
                preprocessCompletedNs = values[offset + 13],
                submissionCompletedNs = values[offset + 14],
                workersCompletedNs = values[offset + 15],
                completedNs = values[offset + 16],
            )
        }
    }
}
