package org.luckypray.dexkit.smali

/** Per-request work/output limits. These are not a limit on total process memory. */
data class SmaliOptions @JvmOverloads constructor(
    val debug: SmaliDebugMode = SmaliDebugMode.NONE,
    val maxOutputBytes: Long = 16L * 1024 * 1024,
    val maxInputBytes: Long = 64L * 1024 * 1024,
    val maxCodeUnits: Long = 1024L * 1024,
    val maxItems: Long = 1024L * 1024,
    val maxAnnotationDepth: Int = 64
) {
    init {
        require(maxOutputBytes in 1..Int.MAX_VALUE.toLong())
        require(maxInputBytes in 1..Int.MAX_VALUE.toLong())
        require(maxCodeUnits in 1..Int.MAX_VALUE.toLong())
        require(maxItems in 1..Int.MAX_VALUE.toLong())
        require(maxAnnotationDepth in 0..256)
    }
}

/** NONE skips method debug parsing; class source and parameter annotations remain. */
enum class SmaliDebugMode(val code: Int) { NONE(0), STRICT(1) }
