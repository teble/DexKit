package org.luckypray.dexkit.query.base

import com.google.flatbuffers.FlatBufferBuilder

abstract class BaseQuery : IQuery {
    @Suppress("INVISIBLE_REFERENCE", "INVISIBLE_MEMBER")
    @kotlin.internal.InlineOnly
    internal abstract fun build(fbb: FlatBufferBuilder): Int

    protected fun getEncodeId(dexId: Int, id: Int) = ((dexId.toLong() shl 32) or id.toLong())
}