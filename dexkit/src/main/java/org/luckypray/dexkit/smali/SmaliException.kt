package org.luckypray.dexkit.smali

enum class SmaliError(val code: Int) {
    OK(0), INVALID_IDENTITY(1), NOT_DEFINED(2), UNSUPPORTED(3), MALFORMED_INPUT(4),
    DEBUG_NOT_REPRESENTABLE(5), LIMIT_EXCEEDED(6), INTERNAL_ERROR(7), BRIDGE_CLOSED(8)
}

enum class SmaliPhase(val code: Int) { RESOLVE(0), READ(1), DECODE(2), EMIT(3) }
enum class SmaliMemberKind(val code: Int) { NONE(0), CLASS(1), FIELD(2), METHOD(3) }

/** Failed requests return no partial smali. Unknown identities/offsets are -1. */
class SmaliException(
    val errorCode: Int,
    val phaseCode: Int,
    val dexId: Long,
    val memberKindCode: Int,
    val memberId: Long,
    val containerByteOffset: Long,
    val codeUnitOffset: Long,
    val detail: Long
) : IllegalStateException(
    "Smali error $errorCode, phase $phaseCode, dex $dexId, member $memberKindCode:$memberId, " +
        "container byte $containerByteOffset, code unit $codeUnitOffset, detail $detail"
) {
    val error: SmaliError get() = SmaliError.values().first { it.code == errorCode }
    val phase: SmaliPhase get() = SmaliPhase.values().first { it.code == phaseCode }
    val memberKind: SmaliMemberKind get() = SmaliMemberKind.values().first { it.code == memberKindCode }
}
