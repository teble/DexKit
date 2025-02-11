@file:Suppress("MemberVisibilityCanBePrivate", "unused")

package org.luckypray.dexkit.query.matchers

import com.google.flatbuffers.FlatBufferBuilder
import org.luckypray.dexkit.InnerMethodsMatcher
import org.luckypray.dexkit.query.base.BaseQuery
import org.luckypray.dexkit.query.enums.MatchType
import org.luckypray.dexkit.query.matchers.base.IntRange

class MethodsMatcher : BaseQuery() {
    private var methods: List<MethodMatcher>? = null
    private var matchType: MatchType = MatchType.Contains
    private var countRange: IntRange? = null

    fun methods(methods: List<MethodMatcher>) = also {
        this.methods = methods
    }

    fun matchType(matchType: MatchType) = also {
        this.matchType = matchType
    }

    fun countRange(countRange: IntRange) = also {
        this.countRange = countRange
    }

    fun countRange(count: Int) = also {
        this.countRange = IntRange(count)
    }

    fun countRange(min: Int, max: Int) = also {
        this.countRange = IntRange(min, max)
    }

    fun add(method: MethodMatcher) = also {
        methods = methods ?: mutableListOf()
        if (methods !is MutableList) {
            methods = methods!!.toMutableList()
        }
        (methods as MutableList<MethodMatcher>).add(method)
    }

    fun add(methodName: String) = also {
        add(MethodMatcher().apply { name(methodName) })
    }

    // region DSL

    fun MethodsMatcher.add(init: MethodMatcher.() -> Unit) = also {
        add(MethodMatcher().apply(init))
    }

    // endregion

    companion object {
        @JvmStatic
        fun create() = MethodsMatcher()
    }

    @Suppress("INVISIBLE_REFERENCE", "INVISIBLE_MEMBER")
    @kotlin.internal.InlineOnly
    override fun build(fbb: FlatBufferBuilder): Int {
        val root = InnerMethodsMatcher.createMethodsMatcher(
            fbb,
            methods?.let { fbb.createVectorOfTables(it.map { it.build(fbb) }.toIntArray()) } ?: 0,
            matchType.value,
            countRange?.build(fbb) ?: 0
        )
        fbb.finish(root)
        return root
    }
}