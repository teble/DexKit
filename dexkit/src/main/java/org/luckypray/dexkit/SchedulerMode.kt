package org.luckypray.dexkit

import org.luckypray.dexkit.annotations.DexKitExperimentalApi

@DexKitExperimentalApi
enum class SchedulerMode(internal val nativeValue: Int) {
    LegacyPerQuery(0),
    SharedPool(1),
}
