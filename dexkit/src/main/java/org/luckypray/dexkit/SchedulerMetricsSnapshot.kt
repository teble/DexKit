package org.luckypray.dexkit

import org.luckypray.dexkit.annotations.DexKitExperimentalApi

@DexKitExperimentalApi
data class SchedulerMetricsSnapshot(
    val shareCountSyncs: Long,
    val shareCountChanges: Long,
    val budgetRebalances: Long,
    val runnableQueueRebuilds: Long,
    val refillRounds: Long,
    val dispatchedTasks: Long,
    val baseDispatchedTasks: Long,
    val bonusDispatchedTasks: Long,
    val maxTotalInFlight: Long,
    val maxVisibleQueryShareCount: Long,
    val maxRunnableQueueSize: Long,
) {
    internal companion object {
        const val NATIVE_SIZE = 11

        fun fromNative(values: LongArray): SchedulerMetricsSnapshot {
            require(values.size == NATIVE_SIZE) {
                "Unexpected scheduler metrics size: ${values.size}"
            }
            return SchedulerMetricsSnapshot(
                shareCountSyncs = values[0],
                shareCountChanges = values[1],
                budgetRebalances = values[2],
                runnableQueueRebuilds = values[3],
                refillRounds = values[4],
                dispatchedTasks = values[5],
                baseDispatchedTasks = values[6],
                bonusDispatchedTasks = values[7],
                maxTotalInFlight = values[8],
                maxVisibleQueryShareCount = values[9],
                maxRunnableQueueSize = values[10],
            )
        }
    }
}
