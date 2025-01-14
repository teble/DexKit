// automatically generated by the FlatBuffers compiler, do not modify

package org.luckypray.dexkit.schema

import com.google.flatbuffers.BaseVector
import com.google.flatbuffers.BooleanVector
import com.google.flatbuffers.ByteVector
import com.google.flatbuffers.Constants
import com.google.flatbuffers.DoubleVector
import com.google.flatbuffers.FlatBufferBuilder
import com.google.flatbuffers.FloatVector
import com.google.flatbuffers.LongVector
import com.google.flatbuffers.StringVector
import com.google.flatbuffers.Struct
import com.google.flatbuffers.Table
import com.google.flatbuffers.UnionVector
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.sign

@Suppress("unused")
internal class `-FindMethod` : Table() {

    fun __init(_i: Int, _bb: ByteBuffer)  {
        __reset(_i, _bb)
    }
    fun __assign(_i: Int, _bb: ByteBuffer) : `-FindMethod` {
        __init(_i, _bb)
        return this
    }
    val searchPackage : String?
        get() {
            val o = __offset(4)
            return if (o != 0) {
                __string(o + bb_pos)
            } else {
                null
            }
        }
    val searchPackageAsByteBuffer : ByteBuffer get() = __vector_as_bytebuffer(4, 1)
    fun searchPackageInByteBuffer(_bb: ByteBuffer) : ByteBuffer = __vector_in_bytebuffer(_bb, 4, 1)
    fun inClasses(j: Int) : Long {
        val o = __offset(6)
        return if (o != 0) {
            bb.getLong(__vector(o) + j * 8)
        } else {
            0
        }
    }
    val inClassesLength : Int
        get() {
            val o = __offset(6); return if (o != 0) __vector_len(o) else 0
        }
    val inClassesAsByteBuffer : ByteBuffer get() = __vector_as_bytebuffer(6, 8)
    fun inClassesInByteBuffer(_bb: ByteBuffer) : ByteBuffer = __vector_in_bytebuffer(_bb, 6, 8)
    fun mutateInClasses(j: Int, inClasses: Long) : Boolean {
        val o = __offset(6)
        return if (o != 0) {
            bb.putLong(__vector(o) + j * 8, inClasses)
            true
        } else {
            false
        }
    }
    fun inMethods(j: Int) : Long {
        val o = __offset(8)
        return if (o != 0) {
            bb.getLong(__vector(o) + j * 8)
        } else {
            0
        }
    }
    val inMethodsLength : Int
        get() {
            val o = __offset(8); return if (o != 0) __vector_len(o) else 0
        }
    val inMethodsAsByteBuffer : ByteBuffer get() = __vector_as_bytebuffer(8, 8)
    fun inMethodsInByteBuffer(_bb: ByteBuffer) : ByteBuffer = __vector_in_bytebuffer(_bb, 8, 8)
    fun mutateInMethods(j: Int, inMethods: Long) : Boolean {
        val o = __offset(8)
        return if (o != 0) {
            bb.putLong(__vector(o) + j * 8, inMethods)
            true
        } else {
            false
        }
    }
    val matcher : `-MethodMatcher`? get() = matcher(`-MethodMatcher`())
    fun matcher(obj: `-MethodMatcher`) : `-MethodMatcher`? {
        val o = __offset(10)
        return if (o != 0) {
            obj.__assign(__indirect(o + bb_pos), bb)
        } else {
            null
        }
    }
    companion object {
        fun validateVersion() = Constants.FLATBUFFERS_23_5_26()
        fun getRootAsFindMethod(_bb: ByteBuffer): `-FindMethod` = getRootAsFindMethod(_bb, `-FindMethod`())
        fun getRootAsFindMethod(_bb: ByteBuffer, obj: `-FindMethod`): `-FindMethod` {
            _bb.order(ByteOrder.LITTLE_ENDIAN)
            return (obj.__assign(_bb.getInt(_bb.position()) + _bb.position(), _bb))
        }
        fun createFindMethod(builder: FlatBufferBuilder, searchPackageOffset: Int, inClassesOffset: Int, inMethodsOffset: Int, matcherOffset: Int) : Int {
            builder.startTable(4)
            addMatcher(builder, matcherOffset)
            addInMethods(builder, inMethodsOffset)
            addInClasses(builder, inClassesOffset)
            addSearchPackage(builder, searchPackageOffset)
            return endFindMethod(builder)
        }
        fun startFindMethod(builder: FlatBufferBuilder) = builder.startTable(4)
        fun addSearchPackage(builder: FlatBufferBuilder, searchPackage: Int) = builder.addOffset(0, searchPackage, 0)
        fun addInClasses(builder: FlatBufferBuilder, inClasses: Int) = builder.addOffset(1, inClasses, 0)
        fun createInClassesVector(builder: FlatBufferBuilder, data: LongArray) : Int {
            builder.startVector(8, data.size, 8)
            for (i in data.size - 1 downTo 0) {
                builder.addLong(data[i])
            }
            return builder.endVector()
        }
        fun startInClassesVector(builder: FlatBufferBuilder, numElems: Int) = builder.startVector(8, numElems, 8)
        fun addInMethods(builder: FlatBufferBuilder, inMethods: Int) = builder.addOffset(2, inMethods, 0)
        fun createInMethodsVector(builder: FlatBufferBuilder, data: LongArray) : Int {
            builder.startVector(8, data.size, 8)
            for (i in data.size - 1 downTo 0) {
                builder.addLong(data[i])
            }
            return builder.endVector()
        }
        fun startInMethodsVector(builder: FlatBufferBuilder, numElems: Int) = builder.startVector(8, numElems, 8)
        fun addMatcher(builder: FlatBufferBuilder, matcher: Int) = builder.addOffset(3, matcher, 0)
        fun endFindMethod(builder: FlatBufferBuilder) : Int {
            val o = builder.endTable()
            return o
        }
    }
}