// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: Apache-2.0
#include <jni.h>
#include <new>
#include <vector>

#include "dexkit.h"

namespace {
void Throw(JNIEnv* env, const char* name, const char* message) {
    if (env->ExceptionCheck()) return;
    jclass type = env->FindClass(name);
    if (!type) return;
    env->ThrowNew(type, message);
    env->DeleteLocalRef(type);
}

void ThrowStatus(JNIEnv* env, const dexkit::SmaliStatus& status) {
    if (env->ExceptionCheck()) return;
    jclass type = env->FindClass("org/luckypray/dexkit/smali/SmaliException");
    if (!type) return;
    auto constructor = env->GetMethodID(type, "<init>", "(IIJIJJJJ)V");
    if (constructor) {
        auto exception = static_cast<jthrowable>(env->NewObject(type, constructor,
            jint(status.error), jint(status.phase), status.dex_id == UINT32_MAX ? jlong(-1) : jlong(status.dex_id),
            jint(status.member_kind), status.member_id == UINT32_MAX ? jlong(-1) : jlong(status.member_id),
            status.dex_offset == UINT64_MAX ? jlong(-1) : jlong(status.dex_offset),
            status.code_offset == UINT32_MAX ? jlong(-1) : jlong(status.code_offset), jlong(status.detail)));
        if (exception) { env->Throw(exception); env->DeleteLocalRef(exception); }
    }
    env->DeleteLocalRef(type);
}

jstring Utf8String(JNIEnv* env, const std::string& text) {
    std::vector<jchar> chars;
    chars.reserve(text.size()); // UTF-16 units cannot exceed UTF-8 bytes.
    for (size_t i = 0; i < text.size();) {
        uint32_t cp = static_cast<uint8_t>(text[i++]);
        unsigned tail = 0;
        uint32_t minimum = 0;
        if (cp >= 0x80) {
            if ((cp & 0xe0) == 0xc0) { cp &= 31; tail = 1; minimum = 0x80; }
            else if ((cp & 0xf0) == 0xe0) { cp &= 15; tail = 2; minimum = 0x800; }
            else if ((cp & 0xf8) == 0xf0) { cp &= 7; tail = 3; minimum = 0x10000; }
            else { ThrowStatus(env, {dexkit::SmaliError::InternalError}); return nullptr; }
            if (tail > text.size() - i) { ThrowStatus(env, {dexkit::SmaliError::InternalError}); return nullptr; }
            for (unsigned j = 0; j < tail; ++j) {
                uint8_t next = text[i++];
                if ((next & 0xc0) != 0x80) { ThrowStatus(env, {dexkit::SmaliError::InternalError}); return nullptr; }
                cp = (cp << 6) | (next & 63);
            }
            if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
                ThrowStatus(env, {dexkit::SmaliError::InternalError}); return nullptr;
            }
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            chars.push_back(0xd800 | (cp >> 10));
            chars.push_back(0xdc00 | (cp & 1023));
        } else chars.push_back(cp);
    }
    const jchar empty = 0;
    return env->NewString(chars.empty() ? &empty : chars.data(), static_cast<jsize>(chars.size()));
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_org_luckypray_dexkit_DexKitBridge_nativeGetSmali(
        JNIEnv* env, jclass, jlong native_ptr, jlong encoded_id, jboolean method,
        jint debug, jlong max_output, jlong max_input, jlong max_code, jlong max_items, jint max_depth) {
#if defined(__cpp_exceptions)
    try {
#endif
        if (!native_ptr) { ThrowStatus(env, {dexkit::SmaliError::BridgeClosed}); return nullptr; }
        auto valid_limit = [](jlong value) { return value > 0 && value <= INT32_MAX && uint64_t(value) <= SIZE_MAX; };
        if ((debug != 0 && debug != 1) || !valid_limit(max_output) || !valid_limit(max_input) ||
            !valid_limit(max_code) || !valid_limit(max_items) || max_depth < 0 || max_depth > 256) {
            Throw(env, "java/lang/IllegalArgumentException", "Invalid smali options");
            return nullptr;
        }
        dexkit::SmaliOptions options{static_cast<dexkit::SmaliDebugMode>(debug), size_t(max_output),
            size_t(max_input), size_t(max_code), size_t(max_items), uint32_t(max_depth)};
        auto* bridge = reinterpret_cast<dexkit::DexKit*>(native_ptr);
        std::string output;
        auto status = method ? bridge->GetMethodSmali(uint64_t(encoded_id), options, output)
                             : bridge->GetClassSmali(uint64_t(encoded_id), options, output);
        if (!status.ok()) { ThrowStatus(env, status); return nullptr; }
        if (output.size() > INT32_MAX) { ThrowStatus(env, {dexkit::SmaliError::LimitExceeded}); return nullptr; }
        return Utf8String(env, output);
#if defined(__cpp_exceptions)
    } catch (const std::bad_alloc&) {
        Throw(env, "java/lang/OutOfMemoryError", "Native smali allocation failed");
    } catch (...) {
        ThrowStatus(env, {dexkit::SmaliError::InternalError});
    }
    return nullptr;
#endif
}
