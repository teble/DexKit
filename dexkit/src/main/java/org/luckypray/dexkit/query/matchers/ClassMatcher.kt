@file:Suppress("MemberVisibilityCanBePrivate", "unused")

package org.luckypray.dexkit.query.matchers

import com.google.flatbuffers.FlatBufferBuilder
import org.luckypray.dexkit.InnerClassMatcher
import org.luckypray.dexkit.query.StringMatcherList
import org.luckypray.dexkit.query.base.BaseQuery
import org.luckypray.dexkit.query.enums.MatchType
import org.luckypray.dexkit.query.enums.StringMatchType
import org.luckypray.dexkit.query.matchers.base.AccessFlagsMatcher
import org.luckypray.dexkit.query.matchers.base.StringMatcher

class ClassMatcher : BaseQuery() {
    private var source: StringMatcher? = null
    private var className: StringMatcher? = null
    private var modifiers: AccessFlagsMatcher? = null
    private var superClass: ClassMatcher? = null
    private var interfaces: InterfacesMatcher? = null
    private var annotations: AnnotationsMatcher? = null
    private var fields: FieldsMatcher? = null
    private var methods: MethodsMatcher? = null
    private var useStrings: List<StringMatcher>? = null

    fun sourceName(matcher: StringMatcher) = also {
        this.source = matcher
    }

    @JvmOverloads
    fun source(
        source: String,
        matchType: StringMatchType = StringMatchType.Equal,
        ignoreCase: Boolean = false
    ) = also { this.source = StringMatcher(source, matchType, ignoreCase) }

    fun className(matcher: StringMatcher) = also { this.className = matcher }

    @JvmOverloads
    fun className(
        className: String,
        matchType: StringMatchType = StringMatchType.Equal,
        ignoreCase: Boolean = false
    ) = also {
        this.className = StringMatcher(className, matchType, ignoreCase)
    }

    fun modifiers(matcher: AccessFlagsMatcher) = also {
        this.modifiers = matcher
    }

    @JvmOverloads
    fun modifiers(modifiers: Int, matchType: MatchType = MatchType.Equal) = also {
        this.modifiers = AccessFlagsMatcher(modifiers, matchType)
    }

    fun superClass(superClass: ClassMatcher) = also {
        this.superClass = superClass
    }

    @JvmOverloads
    fun superClass(
        className: String,
        matchType: StringMatchType = StringMatchType.Equal,
        ignoreCase: Boolean = false
    ) = also {
        this.superClass = ClassMatcher().className(StringMatcher(className, matchType, ignoreCase))
    }

    fun interfaces(interfaces: InterfacesMatcher) = also {
        this.interfaces = interfaces
    }

    fun addInterface(interfaceMatcher: ClassMatcher) = also {
        this.interfaces = this.interfaces ?: InterfacesMatcher()
        this.interfaces!!.add(interfaceMatcher)
    }

    @JvmOverloads
    fun addInterface(
        className: String,
        matchType: StringMatchType = StringMatchType.Equal,
        ignoreCase: Boolean = false
    ) = also {
        this.interfaces = this.interfaces ?: InterfacesMatcher()
        this.interfaces!!.add(ClassMatcher().className(className, matchType, ignoreCase))
    }

    fun interfaceCount(count: Int) = also {
        this.interfaces = this.interfaces ?: InterfacesMatcher()
        this.interfaces!!.countRange(count)
    }

    fun interfaceCount(min: Int, max: Int) = also {
        this.interfaces = this.interfaces ?: InterfacesMatcher()
        this.interfaces!!.countRange(min, max)
    }

    fun annotations(annotations: AnnotationsMatcher) = also {
        this.annotations = annotations
    }

    fun addAnnotation(annotationMatcher: AnnotationMatcher) = also {
        this.annotations = this.annotations ?: AnnotationsMatcher()
        this.annotations!!.add(annotationMatcher)
    }

    fun annotationCount(count: Int) = also {
        this.annotations = this.annotations ?: AnnotationsMatcher()
        this.annotations!!.countRange(count)
    }

    fun annotationCount(min: Int, max: Int) = also {
        this.annotations = this.annotations ?: AnnotationsMatcher()
        this.annotations!!.countRange(min, max)
    }

    fun fields(fields: FieldsMatcher) = also {
        this.fields = fields
    }

    fun addField(fieldMatcher: FieldMatcher) = also {
        this.fields = this.fields ?: FieldsMatcher()
        this.fields!!.add(fieldMatcher)
    }

    fun fieldCount(count: Int) = also {
        this.fields = this.fields ?: FieldsMatcher()
        this.fields!!.countRange(count)
    }

    fun fieldCount(min: Int, max: Int) = also {
        this.fields = this.fields ?: FieldsMatcher()
        this.fields!!.countRange(min, max)
    }

    fun methods(methods: MethodsMatcher) = also {
        this.methods = methods
    }

    fun addMethod(methodMatcher: MethodMatcher) = also {
        this.methods = this.methods ?: MethodsMatcher()
        this.methods!!.add(methodMatcher)
    }

    fun methodCount(count: Int) = also {
        this.methods = this.methods ?: MethodsMatcher()
        this.methods!!.countRange(count)
    }

    fun methodCount(min: Int, max: Int) = also {
        this.methods = this.methods ?: MethodsMatcher()
        this.methods!!.countRange(min, max)
    }

    fun useStringsMatcher(useStrings: List<StringMatcher>) = also {
        this.useStrings = useStrings
    }

    fun useStrings(useStrings: List<String>) = also {
        this.useStrings = useStrings.map { StringMatcher(it) }
    }

    fun addUseString(useString: StringMatcher) = also {
        useStrings = useStrings ?: StringMatcherList()
        if (useStrings !is StringMatcherList) {
            useStrings = StringMatcherList(useStrings!!)
        }
        (useStrings as MutableList<StringMatcher>).add(useString)
    }

    @JvmOverloads
    fun addUseString(
        useString: String,
        matchType: StringMatchType = StringMatchType.Contains,
        ignoreCase: Boolean = false
    ) = also {
        addUseString(StringMatcher(useString, matchType, ignoreCase))
    }

    fun useStrings(vararg useStrings: String) = also {
        this.useStrings = useStrings.map { StringMatcher(it) }
    }

    // region DSL

    fun superClass(init: ClassMatcher.() -> Unit) = also {
        superClass(ClassMatcher().apply(init))
    }

    fun interfaces(init: InterfacesMatcher.() -> Unit) = also {
        interfaces(InterfacesMatcher().apply(init))
    }

    fun addInterface(init: ClassMatcher.() -> Unit) = also {
        addInterface(ClassMatcher().apply(init))
    }

    fun annotations(init: AnnotationsMatcher.() -> Unit) = also {
        annotations(AnnotationsMatcher().apply(init))
    }

    fun addAnnotation(init: AnnotationMatcher.() -> Unit) = also {
        addAnnotation(AnnotationMatcher().apply(init))
    }

    fun fields(init: FieldsMatcher.() -> Unit) = also {
        fields(FieldsMatcher().apply(init))
    }

    fun addField(init: FieldMatcher.() -> Unit) = also {
        addField(FieldMatcher().apply(init))
    }

    fun methods(init: MethodsMatcher.() -> Unit) = also {
        methods(MethodsMatcher().apply(init))
    }

    fun addMethod(init: MethodMatcher.() -> Unit) = also {
        addMethod(MethodMatcher().apply(init))
    }

    fun useStringsMatcher(init: StringMatcherList.() -> Unit) = also {
        useStringsMatcher(StringMatcherList().apply(init))
    }

    // endregion

    companion object {
        fun create() = ClassMatcher()
    }

    @Suppress("INVISIBLE_REFERENCE", "INVISIBLE_MEMBER")
    @kotlin.internal.InlineOnly
    override fun build(fbb: FlatBufferBuilder): Int {
        val root = InnerClassMatcher.createClassMatcher(
            fbb,
            source?.build(fbb) ?: 0,
            className?.build(fbb) ?: 0,
            modifiers?.build(fbb) ?: 0,
            superClass?.build(fbb) ?: 0,
            interfaces?.build(fbb) ?: 0,
            annotations?.build(fbb) ?: 0,
            fields?.build(fbb) ?: 0,
            methods?.build(fbb) ?: 0,
            useStrings?.let { fbb.createVectorOfTables(it.map { it.build(fbb) }.toIntArray()) } ?: 0
        )
        fbb.finish(root)
        return root
    }
}