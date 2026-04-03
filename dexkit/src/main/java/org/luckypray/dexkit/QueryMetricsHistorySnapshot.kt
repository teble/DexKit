package org.luckypray.dexkit

import org.luckypray.dexkit.annotations.DexKitExperimentalApi

@DexKitExperimentalApi
data class QueryMetricsHistorySnapshot(
    val droppedRecords: Long,
    val records: List<QueryMetricsRecord>,
) {
    internal companion object {
        private const val NATIVE_HEADER_SIZE = 2
        private const val NATIVE_RECORD_STRIDE = 2 + QueryMetricsSnapshot.NATIVE_SIZE

        fun fromNative(values: LongArray): QueryMetricsHistorySnapshot {
            require(values.size >= NATIVE_HEADER_SIZE) {
                "Unexpected query metrics history size: ${values.size}"
            }

            val recordCount = values[1].toInt()
            require(recordCount >= 0) {
                "Unexpected query metrics history record count: ${values[1]}"
            }

            val expectedSize = NATIVE_HEADER_SIZE + recordCount * NATIVE_RECORD_STRIDE
            require(values.size == expectedSize) {
                "Unexpected query metrics history payload size: ${values.size}, expected: $expectedSize"
            }

            val records = ArrayList<QueryMetricsRecord>(recordCount)
            var offset = NATIVE_HEADER_SIZE
            repeat(recordCount) {
                val kind = QueryMetricsKind.fromNative(values[offset++])
                val priority = QueryMetricsPriority.fromNative(values[offset++])
                val metrics = QueryMetricsSnapshot.fromNative(values, offset)
                offset += QueryMetricsSnapshot.NATIVE_SIZE
                records.add(
                    QueryMetricsRecord(
                        kind = kind,
                        priority = priority,
                        metrics = metrics,
                    )
                )
            }

            return QueryMetricsHistorySnapshot(
                droppedRecords = values[0],
                records = records,
            )
        }
    }
}
