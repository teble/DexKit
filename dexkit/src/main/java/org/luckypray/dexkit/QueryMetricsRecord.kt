package org.luckypray.dexkit

import org.luckypray.dexkit.annotations.DexKitExperimentalApi

@DexKitExperimentalApi
data class QueryMetricsRecord(
    val kind: QueryMetricsKind,
    val priority: QueryMetricsPriority,
    val metrics: QueryMetricsSnapshot,
)
