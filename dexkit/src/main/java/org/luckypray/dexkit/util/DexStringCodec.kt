// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
package org.luckypray.dexkit.util

import com.google.flatbuffers.FlatBufferBuilder
import java.nio.ByteBuffer

/** Codec for DEX-domain FlatBuffer fields. Opaque labels still use UTF-8. */
internal object DexStringCodec {
    fun create(builder: FlatBufferBuilder, value: String): Int {
        var size = 0L
        for (unit in value) {
            size += when (unit.code) { in 1..127 -> 1; in 0..2047 -> 2; else -> 3 }
        }
        require(size <= Int.MAX_VALUE) { "MUTF-8 string is too large" }
        val bytes = ByteArray(size.toInt())
        // Use the unrestricted encoder. DataOutput's 65535-byte UTF limit
        // does not apply to DEX strings or FlatBuffers string lengths.
        MUtf8Util.encode(bytes, 0, value)
        return builder.createString(ByteBuffer.wrap(bytes))
    }

    fun decode(input: ByteBuffer?): String? {
        if (input == null) return null
        val bytes = input.duplicate()
        val result = StringBuilder(bytes.remaining())
        while (bytes.hasRemaining()) {
            val first = bytes.get().toInt() and 255
            val count: Int
            var unit: Int
            val minimum: Int
            when (first) {
                in 1..127 -> { count = 1; unit = first; minimum = 0 }
                in 0xc0..0xdf -> { count = 2; unit = first and 31; minimum = 128 }
                in 0xe0..0xef -> { count = 3; unit = first and 15; minimum = 2048 }
                else -> throw IllegalArgumentException("Malformed DEX MUTF-8 string")
            }
            require(bytes.remaining() >= count - 1) { "Truncated DEX MUTF-8 string" }
            repeat(count - 1) {
                val tail = bytes.get().toInt() and 255
                require(tail and 0xc0 == 0x80) { "Malformed DEX MUTF-8 continuation" }
                unit = (unit shl 6) or (tail and 63)
            }
            require(unit >= minimum || (count == 2 && unit == 0)) { "Overlong DEX MUTF-8 string" }
            // JVM strings can preserve each UTF-16 unit, including unpaired
            // surrogates. Display escaping is separate from this data API.
            result.append(unit.toChar())
        }
        return result.toString()
    }
}
