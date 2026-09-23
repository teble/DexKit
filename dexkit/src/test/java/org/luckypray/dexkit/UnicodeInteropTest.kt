package org.luckypray.dexkit

import com.google.flatbuffers.FlatBufferBuilder
import com.google.flatbuffers.Utf8
import org.junit.Assert.*
import org.junit.Test
import org.luckypray.dexkit.query.BatchFindMethodUsingStrings
import org.luckypray.dexkit.query.FindMethod
import org.luckypray.dexkit.query.matchers.MethodMatcher
import org.luckypray.dexkit.query.matchers.StringMatchersGroup
import org.luckypray.dexkit.query.enums.StringMatchType
import org.luckypray.dexkit.util.DexStringCodec
import java.nio.ByteBuffer

class UnicodeInteropTest {
    companion object {
        init { System.loadLibrary("dexkit") }
        private val nul = "nul\u0000end"
        private val emoji = "pair\ud83d\ude00"
        private val lone = "lone\ud800Z"
        private val fixture by lazy { SmaliOutputTest.assemble("""
            .class public Lunicode/Probe;
            .super Ljava/lang/Object;
            .source "source\u0000\ud83d\ude00"
            .annotation runtime Lunicode/Marker;
                nul = "nul\u0000end"
                emoji = "pair\ud83d\ude00"
                array = {"nul\u0000end", "pair\ud83d\ude00", "lone\ud800Z"}
                negativeFloat = -0.0f
                positiveFloat = 0.0f
                negativeDouble = -0.0
                positiveDouble = 0.0
            .end annotation
            .method public static strings()V
                .registers 1
                const-string v0, "ascii"
                const-string v0, "bmp\u20ac"
                const-string v0, "nul\u0000end"
                const-string v0, "pair\ud83d\ude00"
                const-string v0, "lone\ud800Z"
                return-void
            .end method
        """.trimIndent()) }
    }

    @Test fun queriesAndJniStringsAgree() {
        val codec = Utf8.getDefault()
        DexKitBridge.create(arrayOf(fixture)).use { bridge ->
            for (value in listOf("ascii", "bmp\u20ac", nul, emoji, lone)) {
                val result = bridge.findMethod(FindMethod.create().matcher(MethodMatcher.create().addEqString(value)))
                assertEquals(1, result.size)
                assertTrue(result.single().usingStrings.contains(value))
                assertTrue(bridge.findMethod(FindMethod.create().matcher(MethodMatcher.create().addEqString(value + "missing"))).isEmpty())
            }
        }
        assertSame(codec, Utf8.getDefault())
        assertEquals(4, codec.encodedLength("\ud83d\ude00"))
    }

    @Test fun scalarAndArrayAnnotationsReturnActualStrings() {
        DexKitBridge.create(arrayOf(fixture)).use { bridge ->
            val type = bridge.findClass { }.single()
            assertEquals("source\u0000\ud83d\ude00", type.sourceFile)
            val elements = type.annotations.single().elements.associate { it.name to it.value }
            assertEquals(nul, elements.getValue("nul").stringValue())
            assertEquals(emoji, elements.getValue("emoji").stringValue())
            assertEquals(listOf(nul, emoji, lone), elements.getValue("array").arrayValue().values.map { it.stringValue() })
        }
    }

    @Test fun opaqueBatchLabelsRemainStandardUtf8() {
        val label = "group\u0000\ud83d\ude00"
        DexKitBridge.create(arrayOf(fixture)).use { bridge ->
            val result = bridge.batchFindMethodUsingStrings(BatchFindMethodUsingStrings.create()
                .addSearchGroup(StringMatchersGroup.create().groupName(label).add(emoji, StringMatchType.Equals)))
            assertEquals(setOf(label), result.keys)
            assertEquals(1, result.getValue(label).size)
        }
    }

    @Test fun annotationFloatingPointBitsPreserveSignedZero() {
        DexKitBridge.create(arrayOf(fixture)).use { bridge ->
            val values = bridge.findClass { }.single().annotations.single().elements.associate { it.name to it.value }
            assertEquals(Int.MIN_VALUE, values.getValue("negativeFloat").floatValue().toRawBits())
            assertEquals(0, values.getValue("positiveFloat").floatValue().toRawBits())
            assertEquals(Long.MIN_VALUE, values.getValue("negativeDouble").doubleValue().toRawBits())
            assertEquals(0L, values.getValue("positiveDouble").doubleValue().toRawBits())
        }
    }

    @Test fun codecPreservesLongStringsAndBufferPositions() {
        val value = "\u0000\ud83d\ude00".repeat(20000)
        val builder = FlatBufferBuilder()
        val text = DexStringCodec.create(builder, value)
        builder.finish(text)
        val buffer = builder.dataBuffer()
        val root = buffer.position() + buffer.getInt(buffer.position())
        val size = buffer.getInt(root)
        val bytes = buffer.duplicate().apply { position(root + 4); limit(root + 4 + size) }
        val position = bytes.position()
        assertEquals(value, DexStringCodec.decode(bytes))
        assertEquals(position, bytes.position())
    }

    @Test fun malformedMutf8DoesNotBecomeEmptyText() {
        for (bytes in listOf(byteArrayOf(0), byteArrayOf(0xc0.toByte()), byteArrayOf(0xc1.toByte(), 0x81.toByte()))) {
            assertThrows(IllegalArgumentException::class.java) { DexStringCodec.decode(ByteBuffer.wrap(bytes)) }
        }
    }
}
