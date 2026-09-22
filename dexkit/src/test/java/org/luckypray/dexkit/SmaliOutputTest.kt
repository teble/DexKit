package org.luckypray.dexkit

import org.jf.dexlib2.Opcodes
import org.jf.dexlib2.dexbacked.DexBackedDexFile
import org.jf.dexlib2.iface.ClassDef
import org.jf.dexlib2.iface.Method
import org.jf.dexlib2.iface.debug.*
import org.jf.dexlib2.iface.instruction.*
import org.jf.dexlib2.immutable.ImmutableAnnotation
import org.jf.dexlib2.immutable.value.ImmutableEncodedValueFactory
import org.jf.smali.Smali
import org.jf.smali.SmaliOptions as AssemblerOptions
import org.junit.Assert.*
import org.junit.Test
import org.luckypray.dexkit.annotations.DexKitExperimentalApi
import org.luckypray.dexkit.smali.SmaliError
import org.luckypray.dexkit.smali.SmaliException
import org.luckypray.dexkit.smali.SmaliOptions
import org.luckypray.dexkit.smali.SmaliDebugMode
import java.nio.file.Files

@OptIn(DexKitExperimentalApi::class)
class SmaliOutputTest {
    companion object {
        init { loadLibrary("dexkit") }

        fun assemble(source: String): ByteArray {
            val directory = Files.createTempDirectory("dexkit-smali-roundtrip").toFile()
            try {
                val file = directory.resolve("Test.smali").apply { writeText(source) }
                val output = directory.resolve("classes.dex")
                val options = AssemblerOptions().apply {
                    apiLevel = 28
                    jobs = 1
                    outputDexFile = output.absolutePath
                }
                assertTrue("Assemble generated smali:\n$source", Smali.assemble(options, file.absolutePath))
                return output.readBytes()
            } finally { directory.deleteRecursively() }
        }

        private fun source(name: String) = SmaliOutputTest::class.java.getResource("/$name")!!.readText()
        private fun read(bytes: ByteArray) = DexBackedDexFile(Opcodes.forApi(28), bytes).classes.single()
        private fun instruction(value: Instruction): List<Any?> = arrayListOf<Any?>().apply {
            add(value.opcode)
            if (value is OneRegisterInstruction) add(value.registerA)
            if (value is TwoRegisterInstruction) add(value.registerB)
            if (value is ThreeRegisterInstruction) add(value.registerC)
            if (value is FiveRegisterInstruction)
                add(listOf(value.registerC, value.registerD, value.registerE, value.registerF, value.registerG).take(value.registerCount))
            if (value is RegisterRangeInstruction) {
                add(value.registerCount)
                add(if (value.registerCount == 0) 0 else value.startRegister)
            }
            if (value is WideLiteralInstruction) add(value.wideLiteral)
            if (value is ReferenceInstruction) add(value.reference.toString())
            if (value is DualReferenceInstruction) add(value.reference2.toString())
            if (value is OffsetInstruction) add(value.codeOffset)
            if (value is org.jf.dexlib2.iface.instruction.formats.ArrayPayload) {
                add(value.elementWidth)
                add(value.arrayElements.toList())
            }
            if (value is SwitchPayload) add(value.switchElements.map { listOf(it.key, it.offset) })
        }

        private fun normalized(value: ClassDef): List<Any?> = listOf(
            value.type, value.accessFlags, value.superclass, value.interfaces, value.sourceFile,
            value.annotations.map { ImmutableAnnotation.of(it) }.toSet(),
            value.fields.map { field -> listOf(field.name, field.type, field.accessFlags,
                field.initialValue?.let { ImmutableEncodedValueFactory.of(it) },
                field.annotations.map { ImmutableAnnotation.of(it) }.toSet()) }.sortedBy { it.toString() },
            value.methods.map { method -> listOf(method.name, method.parameterTypes, method.returnType,
                method.accessFlags, method.annotations.map { ImmutableAnnotation.of(it) }.toSet(),
                method.parameters.map { p -> p.annotations.map { ImmutableAnnotation.of(it) }.toSet() },
                method.implementation?.let { body -> listOf(body.registerCount,
                    body.instructions.map { instruction(it) }, body.tryBlocks.map { block ->
                        listOf(block.startCodeAddress, block.codeUnitCount,
                            block.exceptionHandlers.map { listOf(it.exceptionType, it.handlerCodeAddress) }) }) })
            }.sortedBy { it.take(3).toString() }
        )

        private fun debug(method: Method): List<Any?> = listOf(
            method.parameters.map { it.name },
            method.implementation?.debugItems?.map { item -> arrayListOf<Any?>(item.debugItemType, item.codeAddress).apply {
                if (item is LineNumber) add(item.lineNumber)
                if (item is LocalInfo) { add(item.name); add(item.type); add(item.signature) }
                if (item is StartLocal) add(item.register)
                if (item is EndLocal) add(item.register)
                if (item is RestartLocal) add(item.register)
                if (item is SetSourceFile) add(item.sourceFile)
            } }
        )

        private fun codeOffset(method: Method): Int = method.implementation!!.let { implementation ->
            implementation.javaClass.getDeclaredField("codeOffset").apply { isAccessible = true }.getInt(implementation)
        }

        private fun word(bytes: ByteArray, offset: Int): Int =
            java.nio.ByteBuffer.wrap(bytes).order(java.nio.ByteOrder.LITTLE_ENDIAN).getInt(offset)
        private fun putWord(bytes: ByteArray, offset: Int, value: Int) {
            java.nio.ByteBuffer.wrap(bytes).order(java.nio.ByteOrder.LITTLE_ENDIAN).putInt(offset, value)
        }

        private fun section(bytes: ByteArray, kind: Int): Int {
            val map = word(bytes, 52)
            for (i in 0 until word(bytes, map)) {
                val item = map + 4 + i * 12
                if ((bytes[item].toInt() and 255) or ((bytes[item + 1].toInt() and 255) shl 8) == kind)
                    return word(bytes, item + 8)
            }
            error("No map section $kind")
        }
    }

    @Test fun completeClassRoundTripPreservesSemantics() {
        for (resource in listOf("SmaliRoundTrip.smali", "ParameterAnnotations.smali", "SmaliBoundaries.smali")) {
            val bytes = assemble(source(resource))
            val original = read(bytes)
            DexKitBridge.create(arrayOf(bytes)).use { bridge ->
                bridge.setMaxConcurrentQueries(1)
                val text = bridge.getClassData(original.type)!!.toSmali()
                assertEquals(normalized(original), normalized(read(assemble(text))))
            }
        }
    }

    @Test fun failuresDoNotInvalidateBridgeAndTextOutlivesIt() {
        val bytes = assemble(source("SmaliRoundTrip.smali"))
        val bridge = DexKitBridge.create(arrayOf(bytes))
        val method = bridge.getMethodData("Ltest/SmaliRoundTrip;->target()V")!!
        try {
            method.toSmali(SmaliOptions(maxOutputBytes = 2))
            fail("Expected output limit")
        } catch (error: SmaliException) { assertEquals(SmaliError.LIMIT_EXCEEDED, error.error) }
        val text = method.toSmali()
        bridge.close()
        assertTrue(text.contains("return-void"))
        try { method.toSmali(); fail("Expected bridge closed") }
        catch (error: SmaliException) { assertEquals(SmaliError.BRIDGE_CLOSED, error.error) }
    }

    @Test fun classAnnotationDirectoryFitsACumulativeWorkBudget() {
        val source = ".class public Ltest/Large;\n.super Ljava/lang/Object;\n" +
            (0 until 100).joinToString("\n") { index ->
                ".method public static m$index()V\n.registers 0\n" +
                    ".annotation runtime Ltest/Marker;\n.end annotation\nreturn-void\n.end method\n"
            }
        val bytes = assemble(source)
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            val text = bridge.getClassData("Ltest/Large;")!!.toSmali(SmaliOptions(maxItems = 2000, maxCodeUnits = 1))
            assertEquals(normalized(read(bytes)), normalized(read(assemble(text))))
        }
    }

    @Test fun strictDebugRoundTripPreservesInterpretedEventsAndNames() {
        val bytes = assemble(source("SmaliDebug.smali"))
        val original = read(bytes)
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            val data = bridge.getClassData(original.type)!!
            val result = read(assemble(data.toSmali(SmaliOptions(debug = SmaliDebugMode.STRICT))))
            assertEquals(normalized(original), normalized(result))
            for (method in original.methods) assertEquals(method.name,
                debug(method), debug(result.methods.single { it.name == method.name }))
            val without = read(assemble(data.toSmali()))
            assertEquals("Debug.java", without.sourceFile)
            assertTrue(without.methods.all { it.implementation!!.debugItems.none() })
            assertTrue(without.methods.all { it.parameters.all { p -> p.name == null } })
        }
    }

    @Test fun sharedLongDebugAndTruncatedDebugAreDifferentFailures() {
        val original = assemble(source("SmaliDebug.smali"))
        val definition = read(original)
        val short = codeOffset(definition.methods.single { it.name == "shortMethod" })
        val long = codeOffset(definition.methods.single { it.name == "longMethod" })
        val shared = original.copyOf()
        putWord(shared, short + 8, word(shared, long + 8))
        val truncated = original.copyOf()
        putWord(truncated, short + 8, truncated.size - 1)
        truncated[truncated.lastIndex] = 1
        for ((bytes, expected) in listOf(shared to SmaliError.DEBUG_NOT_REPRESENTABLE,
                                         truncated to SmaliError.MALFORMED_INPUT)) {
            DexKitBridge.create(arrayOf(bytes)).use { bridge ->
                val method = bridge.getMethodData("Ltest/SmaliDebug;->shortMethod()V")!!
                assertTrue(method.toSmali().contains("return-void"))
                try { method.toSmali(SmaliOptions(debug = SmaliDebugMode.STRICT)); fail("Expected $expected") }
                catch (error: SmaliException) { assertEquals(expected, error.error) }
                assertTrue(method.toSmali().contains("return-void"))
            }
        }
    }

    @Test fun classRejectsAnnotationsForForeignOrUndefinedMembers() {
        val text = """
            .class public Ltest/Directory;
            .super Ljava/lang/Object;
            .field public static present:I
                .annotation runtime Ltest/Marker;
                .end annotation
            .end field
            .method public static good()V
                .registers 1
                sget v0, Ltest/Directory;->missing:I
                sget v0, Lother/External;->value:I
                return-void
            .end method
        """.trimIndent()
        val original = assemble(text)
        val dex = DexBackedDexFile(Opcodes.forApi(28), original)
        assertEquals(1, dex.classes.size)
        val classDefOffset = word(original, 100) // header.class_defs_off, one class
        val directory = word(original, classDefOffset + 20)
        for (field in listOf("missing", "value")) {
            val bytes = original.copyOf()
            val index = dex.fieldSection.indexOfFirst { it.name == field }
            assertTrue(index >= 0)
            putWord(bytes, directory + 16, index)
            DexKitBridge.create(arrayOf(bytes)).use { bridge ->
                assertTrue(bridge.getMethodData("Ltest/Directory;->good()V")!!.toSmali().contains("return-void"))
                try { bridge.getClassData("Ltest/Directory;")!!.toSmali(); fail("Expected orphan annotation failure") }
                catch (error: SmaliException) { assertEquals(SmaliError.MALFORMED_INPUT, error.error) }
            }
        }
    }

    @Test fun distinctEqualHandlesAreRejectedButRepeatedIdentitySurvives() {
        val source = """
            .class public Ltest/Handles;
            .super Ljava/lang/Object;
            .method public static handles()V
                .registers 2
                const-method-handle v0, invoke-static@Lother/Target;->a()V
                const-method-handle v1, invoke-static@Lother/Target;->b()V
                return-void
            .end method
        """.trimIndent()
        val original = assemble(source)
        val handles = section(original, 8)
        val code = codeOffset(read(original).methods.single())
        val duplicate = original.copyOf()
        original.copyInto(duplicate, handles + 8, handles, handles + 8)
        DexKitBridge.create(arrayOf(duplicate)).use { bridge ->
            val method = bridge.getMethodData("Ltest/Handles;->handles()V")!!
            try { method.toSmali(); fail("Distinct handle identities must not merge") }
            catch (error: SmaliException) {
                assertEquals(SmaliError.UNSUPPORTED, error.error)
                assertEquals((handles + 8).toLong(), error.containerByteOffset)
            }
        }
        // Both instructions now reference the first original ID. An unused
        // duplicate table entry must not prevent this method from being output.
        val shared = duplicate.copyOf()
        shared[code + 22] = shared[code + 18]
        shared[code + 23] = shared[code + 19]
        DexKitBridge.create(arrayOf(shared)).use { bridge ->
            val result = read(assemble(bridge.getClassData("Ltest/Handles;")!!.toSmali()))
            val insns = result.methods.single().implementation!!.instructions.toList()
            val references = insns.filterIsInstance<ReferenceInstruction>()
            assertEquals(references[0].reference, references[1].reference)
        }
    }

    @Test fun staticValueTypeMismatchIsRejectedWithoutAffectingMethodOutput() {
        val original = assemble("""
            .class public Ltest/Values;
            .super Ljava/lang/Object;
            .field public static a:I = 1
            .field public static z:Ljava/lang/String; = "abc"
            .method public static good()V
                .registers 0
                return-void
            .end method
        """.trimIndent())
        val dex = DexBackedDexFile(Opcodes.forApi(28), original)
        val values = word(original, word(original, 100) + 28)
        assertEquals(2, original[values].toInt())
        val malformed = original.copyOf()
        malformed[values + 1] = 0x17 // VALUE_STRING in the I field's slot.
        malformed[values + 2] = dex.stringSection.indexOfFirst { it == "abc" }.toByte()
        DexKitBridge.create(arrayOf(malformed)).use { bridge ->
            try { bridge.getClassData("Ltest/Values;")!!.toSmali(); fail("Expected type mismatch") }
            catch (error: SmaliException) {
                assertEquals(SmaliError.MALFORMED_INPUT, error.error)
                assertEquals((values + 1).toLong(), error.containerByteOffset)
            }
            assertTrue(bridge.getMethodData("Ltest/Values;->good()V")!!.toSmali().contains("return-void"))
        }
    }

    @Test fun invalidSwitchTargetReportsTheReferringInstruction() {
        val bytes = assemble(source("SmaliRoundTrip.smali"))
        val code = codeOffset(read(bytes).methods.single { it.name == "packed" })
        // pc 0 switch -> pc 4 payload; its target must not point inside pc 0.
        putWord(bytes, code + 16 + 4 * 2 + 8, 1)
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            try { bridge.getMethodData("Ltest/SmaliRoundTrip;->packed(I)I")!!.toSmali(); fail("Expected invalid target") }
            catch (error: SmaliException) {
                assertEquals(SmaliError.MALFORMED_INPUT, error.error)
                assertEquals(0L, error.codeUnitOffset)
                assertEquals((code + 16).toLong(), error.containerByteOffset)
            }
        }
    }

    @Test fun exportFixturesForDirectNativeChecks() {
        val directory = java.io.File("build/smali-fixtures").apply { mkdirs() }
        for (name in listOf("SmaliRoundTrip", "SmaliDebug", "ParameterAnnotations", "SmaliBoundaries"))
            directory.resolve("$name.dex").writeBytes(assemble(source("$name.smali")))
    }

    @Test fun callSiteIdentityPartitionsSurviveEqualContents() {
        val bootstrap = "Ltest/Bootstrap;->bootstrap(Ljava/lang/invoke/MethodHandles\$Lookup;" +
            "Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;"
        val bytes = assemble("""
            .class public Ltest/CallSites;
            .super Ljava/lang/Object;
            .method public static calls()V
                .registers 0
                invoke-custom {}, first("a", ()V)@$bootstrap
                invoke-custom {}, first("a", ()V)@$bootstrap
                invoke-custom {}, second("b", ()V)@$bootstrap
                return-void
            .end method
        """.trimIndent())
        val table = section(bytes, 7)
        // Different call-site IDs share exactly the same encoded-array bytes.
        putWord(bytes, table + 4, word(bytes, table))
        fun identities(value: ClassDef) = value.methods.single().implementation!!.instructions
            .filterIsInstance<ReferenceInstruction>().map {
                (it.reference as org.jf.dexlib2.dexbacked.reference.DexBackedCallSiteReference).callSiteIndex
            }
        val before = identities(read(bytes))
        assertEquals(before[0], before[1])
        assertNotEquals(before[0], before[2])
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            val after = identities(read(assemble(bridge.getClassData("Ltest/CallSites;")!!.toSmali())))
            for (i in before.indices) for (j in before.indices)
                assertEquals(before[i] == before[j], after[i] == after[j])
        }
    }

    @Test fun unicodeNamesAndNullDebugLocalsSurvive() {
        val text = source("SmaliBoundaries.smali").replace("Ltest/SmaliBoundaries;", "Ltest/\u6d4b\u8bd5;")
            .replace("nullableLocal", "\u5c40\u90e8")
        val bytes = assemble(text)
        val original = read(bytes)
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            val result = read(assemble(bridge.getClassData(original.type)!!.toSmali(
                SmaliOptions(debug = SmaliDebugMode.STRICT))))
            assertEquals(normalized(original), normalized(result))
            for (method in original.methods)
                assertEquals(debug(method), debug(result.methods.single { it.name == method.name }))
        }
        val dex = DexBackedDexFile(Opcodes.forApi(28), bytes)
        val nameIndex = dex.stringSection.indexOfFirst { it == "\u5c40\u90e8" }
        val nameOffset = word(bytes, word(bytes, 60) + nameIndex * 4)
        assertEquals(2, bytes[nameOffset].toInt())
        // Same UTF-16 length and MUTF-8 byte width, but a supplementary name
        // cannot be reassembled by the supported lexer.
        byteArrayOf(0xed.toByte(), 0xa0.toByte(), 0xbd.toByte(), 0xed.toByte(), 0xb8.toByte(), 0x80.toByte())
            .copyInto(bytes, nameOffset + 1)
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            try { bridge.getClassData(original.type)!!.toSmali(); fail("Expected unsupported supplementary name") }
            catch (error: SmaliException) { assertEquals(SmaliError.UNSUPPORTED, error.error) }
        }
    }

    @Test fun noncanonicalNaNIsRejected() {
        val seed = assemble(".class public Ltest/Nan;\n.super Ljava/lang/Object;\n" +
            ".field public static f:F = 0x1.000002p0f\n")
        val values = word(seed, word(seed, 100) + 28)
        assertEquals(0x70, seed[values + 1].toInt() and 255) // float, all 4 bytes
        for (bits in listOf(0x7fa00001, 0xffc00000.toInt())) {
            val bytes = seed.copyOf()
            putWord(bytes, values + 2, bits)
            DexKitBridge.create(arrayOf(bytes)).use { bridge ->
                try { bridge.getClassData("Ltest/Nan;")!!.toSmali(); fail("NaN payload/sign must not be canonicalized") }
                catch (error: SmaliException) { assertEquals(SmaliError.UNSUPPORTED, error.error) }
            }
        }
        val negativeZero = seed.copyOf()
        putWord(negativeZero, values + 2, Int.MIN_VALUE)
        DexKitBridge.create(arrayOf(negativeZero)).use { bridge ->
            val text = bridge.getClassData("Ltest/Nan;")!!.toSmali()
            assertTrue(text.contains("= -0x0.000000p-126f"))
            val literal = text.lineSequence().single { it.startsWith(".field") }.substringAfter(" = ")
            assertEquals(Int.MIN_VALUE, java.lang.Float.floatToRawIntBits(java.lang.Float.parseFloat(literal)))
            // Explicit oracle limitation: a default-value suffix is trimmed by
            // smali 2.5.2, losing the sign despite the exact emitted literal.
            val after = read(assemble(text)).fields.single().initialValue
            val value = (after as? org.jf.dexlib2.iface.value.FloatEncodedValue)?.value ?: 0f
            assertEquals(0, java.lang.Float.floatToRawIntBits(value))
        }
    }

    @Test fun strictChecksLocalHistoryAndDeepDebugTruncation() {
        val seed = assemble(".class public Ltest/LocalState;\n.super Ljava/lang/Object;\n" +
            ".method public static run()V\n.registers 1\nreturn-void\n.end method\n")
        val code = codeOffset(read(seed).methods.single())
        fun withDebug(vararg stream: Int): ByteArray {
            val bytes = seed.copyOf(seed.size + 16)
            putWord(bytes, 32, bytes.size)
            putWord(bytes, 104, word(seed, 104) + 16)
            val offset = bytes.size - stream.size
            putWord(bytes, code + 8, offset)
            stream.forEachIndexed { index, byte -> bytes[offset + index] = byte.toByte() }
            return bytes
        }
        val invalid = listOf(
            withDebug(1, 0, 6, 0, 0), // restart with no history
            withDebug(1, 0, 5, 0, 0), // end without a live local
            withDebug(1, 0, 3, 0, 0, 0, 5, 0, 5, 0, 0), // double end
            withDebug(1, 0, 3, 0, 0, 0, 6, 0, 0), // restart while live
            withDebug(1, 0, 14, 4, 0, 0, 0, 0x80) // event allocated, signature LEB truncated
        )
        for (bytes in invalid) DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            val method = bridge.getMethodData("Ltest/LocalState;->run()V")!!
            assertTrue(method.toSmali().contains("return-void"))
            try { method.toSmali(SmaliOptions(debug = SmaliDebugMode.STRICT)); fail("Expected malformed local state/stream") }
            catch (error: SmaliException) { assertEquals(SmaliError.MALFORMED_INPUT, error.error) }
        }
        for (bytes in listOf(withDebug(100, 0, 0), withDebug(1, 0, 3, 0, 0, 0, 5, 0, 6, 0, 5, 0, 0))) {
            DexKitBridge.create(arrayOf(bytes)).use { bridge ->
                val text = bridge.getClassData("Ltest/LocalState;")!!.toSmali(SmaliOptions(debug = SmaliDebugMode.STRICT))
                assertFalse(text.contains(".line"))
                assertEquals(debug(read(bytes).methods.single()), debug(read(assemble(text)).methods.single()))
            }
        }
        val parameters = assemble("""
            .class public Ltest/Params;
            .super Ljava/lang/Object;
            .method public params(JI)V
                .registers 4
                .end local p0
                .restart local p0
                .end local p1
                .restart local p1
                .end local p3
                .restart local p3
                return-void
            .end method
        """.trimIndent())
        DexKitBridge.create(arrayOf(parameters)).use { bridge ->
            val result = read(assemble(bridge.getClassData("Ltest/Params;")!!.toSmali(SmaliOptions(debug = SmaliDebugMode.STRICT))))
            assertEquals(debug(read(parameters).methods.single()), debug(result.methods.single()))
        }
    }

    @Test fun floatingValuesPreserveRawBits() {
        val floats = listOf(0, Int.MIN_VALUE, 1, 0x007fffff, 0x00800000, 0x3f800001, 0x7f7fffff,
            0x7f800000, 0xff800000.toInt(), 0x7fc00000)
        val doubles = listOf(0L, Long.MIN_VALUE, 1L, 0x000fffffffffffffL, 0x0010000000000000L,
            0x3ff0000000000001L, 0x7fefffffffffffffL, 0x7ff0000000000000L, -0x10000000000000L, 0x7ff8000000000000L)
        val fields = floats.mapIndexed { i, bits ->
            ".field public static f$i:F = ${java.lang.Float.toHexString(java.lang.Float.intBitsToFloat(bits))}f"
        } + doubles.mapIndexed { i, bits ->
            ".field public static d$i:D = ${java.lang.Double.toHexString(java.lang.Double.longBitsToDouble(bits))}"
        }
        // smali 2.5.2 erroneously trims trailing -0 as a default value. The
        // non-default sentinel keeps this oracle from discarding the input.
        val bytes = assemble(".class public Ltest/Floats;\n.super Ljava/lang/Object;\n" +
            fields.joinToString("\n") + "\n.field public static z:I = 1\n")
        fun bits(value: ClassDef) = value.fields.associate { field -> field.name to when (val v = field.initialValue) {
            is org.jf.dexlib2.iface.value.FloatEncodedValue -> java.lang.Float.floatToRawIntBits(v.value).toLong()
            is org.jf.dexlib2.iface.value.DoubleEncodedValue -> java.lang.Double.doubleToRawLongBits(v.value)
            else -> 1L
        } }
        val expected = floats.mapIndexed { i, v -> "f$i" to v.toLong() }.toMap() +
            doubles.mapIndexed { i, v -> "d$i" to v }.toMap() + ("z" to 1L)
        assertEquals(expected, bits(read(bytes)))
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            assertEquals(expected, bits(read(assemble(bridge.getClassData("Ltest/Floats;")!!.toSmali()))))
        }
    }

    @Test fun standardDexVersionsAndNonzeroDexIdentity() {
        val ordinary = assemble(".class public Ltest/Version;\n.super Ljava/lang/Object;\n" +
            ".method public static run()V\n.registers 0\nreturn-void\n.end method\n")
        val other = assemble(".class public Ltest/Other;\n.super Ljava/lang/Object;\n")
        for (version in listOf("035", "037", "038", "039", "040")) {
            val bytes = ordinary.copyOf()
            version.toByteArray().copyInto(bytes, 4)
            DexKitBridge.create(arrayOf(other, bytes)).use { bridge ->
                val method = bridge.getMethodData("Ltest/Version;->run()V")!!
                assertTrue(method.toSmali().contains("return-void"))
                assertEquals(normalized(read(ordinary)), normalized(read(assemble(
                    bridge.getClassData("Ltest/Version;")!!.toSmali()))))
                try { method.toSmali(SmaliOptions(maxOutputBytes = 1)); fail("Expected limit") }
                catch (error: SmaliException) { assertEquals(1L, error.dexId) }
            }
        }
    }

    @Test fun outputRunsAlongsideQueriesWarmupAndClose() {
        val bytes = assemble(source("SmaliDebug.smali"))
        val bridge = DexKitBridge.create(arrayOf(bytes))
        val method = bridge.getMethodData("Ltest/SmaliDebug;->longMethod()V")!!
        val clazz = bridge.getClassData("Ltest/SmaliDebug;")!!
        val expected = method.toSmali()
        val expectedClass = clazz.toSmali(SmaliOptions(debug = SmaliDebugMode.STRICT))
        val pool = java.util.concurrent.Executors.newFixedThreadPool(4)
        try {
            bridge.setMaxConcurrentQueries(1)
            val start = java.util.concurrent.CountDownLatch(1)
            val work = listOf(
                pool.submit { start.await(); repeat(100) { assertEquals(expected, method.toSmali()) } },
                pool.submit { start.await(); repeat(100) {
                    assertEquals(expectedClass, clazz.toSmali(SmaliOptions(debug = SmaliDebugMode.STRICT)))
                } },
                pool.submit { start.await(); repeat(100) { assertFalse(bridge.findMethod { matcher { name = "longMethod" } }.isEmpty()) } },
                pool.submit { start.await(); bridge.initFullCache() }
            )
            start.countDown()
            work.forEach { it.get(30, java.util.concurrent.TimeUnit.SECONDS) }
            val closeStart = java.util.concurrent.CountDownLatch(1)
            val readers = (0 until 3).map { pool.submit {
                closeStart.await()
                repeat(100) {
                    try { assertEquals(expected, method.toSmali()) }
                    catch (error: SmaliException) { assertEquals(SmaliError.BRIDGE_CLOSED, error.error) }
                }
            } }
            val close = pool.submit { closeStart.await(); bridge.close() }
            closeStart.countDown()
            (readers + close).forEach { it.get(30, java.util.concurrent.TimeUnit.SECONDS) }
            assertTrue(expected.contains("return-void"))
        } finally { bridge.close(); pool.shutdownNow() }
    }

    @Test fun container041UsesPhysicalOffsetsBeyondTheLogicalDex() {
        // Three logical headers. The middle DEX owns the class; its strings,
        // class_data and code are shared data after the last header, outside
        // the middle DEX's [120, 320) logical span.
        val bytes = ByteArray(768)
        fun put(offset: Int, value: Int) = putWord(bytes, offset, value)
        for ((offset, size) in listOf(0 to 120, 120 to 200, 320 to 448)) {
            "dex\n041\u0000".toByteArray().copyInto(bytes, offset)
            put(offset + 32, size); put(offset + 36, 120); put(offset + 40, 0x12345678)
            put(offset + 52, 600); put(offset + 112, bytes.size); put(offset + 116, offset)
        }
        for ((offset, value) in listOf(56 to 4, 60 to 240, 64 to 3, 68 to 256,
            72 to 1, 76 to 268, 88 to 1, 92 to 280, 96 to 1, 100 to 288)) put(120 + offset, value)
        var string = 448
        for ((index, value) in listOf("Ljava/lang/Object;", "Ltest/Container;", "V", "run").withIndex()) {
            put(240 + index * 4, string)
            bytes[string++] = value.length.toByte()
            value.toByteArray().copyInto(bytes, string)
            string += value.length + 1
        }
        put(256, 0); put(260, 1); put(264, 2) // types
        put(268, 2); put(272, 2) // ()V
        put(280, 1); put(284, 3) // Container.run
        put(288, 1); put(292, 1); put(296, 0); put(304, -1); put(312, 560)
        byteArrayOf(0, 0, 1, 0, 0, 9, 0xc0.toByte(), 4).copyInto(bytes, 560)
        put(576 + 12, 1); bytes[592] = 0x0e // return-void
        val sections = listOf(0 to 120, 1 to 240, 2 to 256, 3 to 268, 5 to 280,
            6 to 288, 0x2002 to 448, 0x2000 to 560, 0x2001 to 576, 0x1000 to 600)
        put(600, sections.size)
        for ((i, section) in sections.withIndex()) {
            val entry = 604 + i * 12
            put(entry, section.first); put(entry + 4, when (section.first) { 1, 0x2002 -> 4; 2 -> 3; else -> 1 })
            put(entry + 8, section.second)
        }
        val expected = assemble(".class public Ltest/Container;\n.super Ljava/lang/Object;\n" +
            ".method public static run()V\n.registers 0\nreturn-void\n.end method\n")
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            assertEquals(3, bridge.getDexNum())
            val method = bridge.getMethodData("Ltest/Container;->run()V")!!
            assertTrue(method.toSmali().contains("return-void"))
            assertEquals(normalized(read(expected)), normalized(read(assemble(
                bridge.getClassData("Ltest/Container;")!!.toSmali()))))
        }
        bytes[592] = 0xe3.toByte() // Unsupported runtime-only opcode.
        DexKitBridge.create(arrayOf(bytes)).use { bridge ->
            try { bridge.getMethodData("Ltest/Container;->run()V")!!.toSmali(); fail("Expected unsupported opcode") }
            catch (error: SmaliException) {
                assertEquals(SmaliError.UNSUPPORTED, error.error)
                assertEquals(1L, error.dexId)
                assertEquals(592L, error.containerByteOffset)
                assertEquals(0L, error.codeUnitOffset)
            }
        }
    }
}
