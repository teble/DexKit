package org.luckypray.dexkit

import org.jf.dexlib2.Opcodes
import org.jf.dexlib2.dexbacked.DexBackedDexFile
import org.jf.dexlib2.iface.ClassDef
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
    }

    @Test fun completeClassRoundTripPreservesSemantics() {
        for (resource in listOf("SmaliRoundTrip.smali", "ParameterAnnotations.smali")) {
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
            val text = bridge.getClassData("Ltest/Large;")!!.toSmali(SmaliOptions(maxItems = 2000))
            assertEquals(normalized(read(bytes)), normalized(read(assemble(text))))
        }
    }
}
