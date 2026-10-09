#include "reflection_filter.h"

#include <string>

#include "log.h"

namespace {
// LineageOS adds this public constant to AssetManager; reflection on it is a
// permissionless ROM fingerprint.
constexpr const char *kHiddenField = "LINEAGE_APK_PATH";

using GetFieldByNameFn = jobject (*)(JNIEnv *, jobject, jstring);
using GetFieldsFn = jobjectArray (*)(JNIEnv *, jobject);
using GetFieldsBoolFn = jobjectArray (*)(JNIEnv *, jobject, jboolean);

GetFieldByNameFn g_original_get_declared_field = nullptr;
GetFieldByNameFn g_original_get_public_field = nullptr;
GetFieldsFn g_original_get_declared_fields = nullptr;
GetFieldsBoolFn g_original_get_declared_fields0 = nullptr;
GetFieldsBoolFn g_original_get_declared_fields_unchecked = nullptr;
jmethodID g_field_get_name = nullptr;
bool g_field_hooks_installed = false;

void clear_jni_exception(JNIEnv *env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

std::string jstring_to_ascii(JNIEnv *env, jstring value) {
    if (!env || !value) return {};
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        clear_jni_exception(env);
        return {};
    }
    try {
        std::string out = chars;
        env->ReleaseStringUTFChars(value, chars);
        return out;
    } catch (...) {
        env->ReleaseStringUTFChars(value, chars);
        return {};
    }
}

bool jstring_equals(JNIEnv *env, jstring value, const char *needle) {
    if (!env || !value || !needle) return false;
    return jstring_to_ascii(env, value) == needle;
}

bool field_is_hidden(JNIEnv *env, jobject field) {
    if (!env || !field || !g_field_get_name) return false;
    auto name = static_cast<jstring>(env->CallObjectMethod(field, g_field_get_name));
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        if (name) env->DeleteLocalRef(name);
        return false;
    }
    const bool hidden = name && jstring_equals(env, name, kHiddenField);
    if (name) env->DeleteLocalRef(name);
    return hidden;
}

// Returns the original array or a copy without the hidden field.
jobjectArray filter_field_array(JNIEnv *env, jobjectArray fields) {
    if (!env || !fields || env->ExceptionCheck()) return fields;
    const jsize length = env->GetArrayLength(fields);
    if (env->ExceptionCheck() || length <= 0) {
        clear_jni_exception(env);
        return fields;
    }
    jsize dropped = 0;
    for (jsize index = 0; index < length; ++index) {
        jobject field = env->GetObjectArrayElement(fields, index);
        if (!field) continue;
        if (field_is_hidden(env, field)) ++dropped;
        env->DeleteLocalRef(field);
        if (env->ExceptionCheck()) {
            clear_jni_exception(env);
            return fields;
        }
    }
    if (dropped == 0) return fields;

    jclass field_class = env->FindClass("java/lang/reflect/Field");
    if (!field_class || env->ExceptionCheck()) {
        clear_jni_exception(env);
        return fields;
    }
    jobjectArray filtered = env->NewObjectArray(length - dropped, field_class, nullptr);
    if (!filtered || env->ExceptionCheck()) {
        clear_jni_exception(env);
        return fields;
    }
    jsize write = 0;
    for (jsize index = 0; index < length; ++index) {
        jobject field = env->GetObjectArrayElement(fields, index);
        if (!field) continue;
        if (!field_is_hidden(env, field)) {
            env->SetObjectArrayElement(filtered, write++, field);
            if (env->ExceptionCheck()) {
                clear_jni_exception(env);
                env->DeleteLocalRef(field);
                return fields;
            }
        }
        env->DeleteLocalRef(field);
    }
    return filtered;
}

jobject hook_get_declared_field(JNIEnv *env, jobject clazz, jstring name) {
    if (!g_original_get_declared_field) return nullptr;
    if (jstring_equals(env, name, kHiddenField)) {
        jclass exception = env->FindClass("java/lang/NoSuchFieldException");
        if (exception) env->ThrowNew(exception, kHiddenField);
        return nullptr;
    }
    return g_original_get_declared_field(env, clazz, name);
}

// Class.getField() turns a null result into NoSuchFieldException.
jobject hook_get_public_field(JNIEnv *env, jobject clazz, jstring name) {
    if (!g_original_get_public_field) return nullptr;
    if (jstring_equals(env, name, kHiddenField)) return nullptr;
    return g_original_get_public_field(env, clazz, name);
}

jobjectArray hook_get_declared_fields(JNIEnv *env, jobject clazz) {
    if (!g_original_get_declared_fields) return nullptr;
    return filter_field_array(env, g_original_get_declared_fields(env, clazz));
}

jobjectArray hook_get_declared_fields0(JNIEnv *env, jobject clazz, jboolean public_only) {
    if (!g_original_get_declared_fields0) return nullptr;
    return filter_field_array(env, g_original_get_declared_fields0(env, clazz, public_only));
}

jobjectArray hook_get_declared_fields_unchecked(JNIEnv *env, jobject clazz, jboolean public_only) {
    if (!g_original_get_declared_fields_unchecked) return nullptr;
    return filter_field_array(env,
                              g_original_get_declared_fields_unchecked(env, clazz, public_only));
}
} // namespace

bool install_field_hooks(JNIEnv *env, zygisk::Api *api) {
    if (g_field_hooks_installed) return true;
    if (!env || !api) return false;

    jclass field_class = env->FindClass("java/lang/reflect/Field");
    if (field_class && !env->ExceptionCheck()) {
        g_field_get_name = env->GetMethodID(field_class, "getName", "()Ljava/lang/String;");
    }
    clear_jni_exception(env);

    JNINativeMethod field_methods[] = {
        {"getDeclaredField", "(Ljava/lang/String;)Ljava/lang/reflect/Field;",
         reinterpret_cast<void *>(hook_get_declared_field)},
        {"getPublicFieldRecursive", "(Ljava/lang/String;)Ljava/lang/reflect/Field;",
         reinterpret_cast<void *>(hook_get_public_field)},
        {"getDeclaredFields", "()[Ljava/lang/reflect/Field;",
         reinterpret_cast<void *>(hook_get_declared_fields)},
        {"getDeclaredFields0", "(Z)[Ljava/lang/reflect/Field;",
         reinterpret_cast<void *>(hook_get_declared_fields0)},
        {"getDeclaredFieldsUnchecked", "(Z)[Ljava/lang/reflect/Field;",
         reinterpret_cast<void *>(hook_get_declared_fields_unchecked)},
    };
    api->hookJniNativeMethods(env, "java/lang/Class", field_methods,
                              static_cast<int>(sizeof(field_methods) / sizeof(field_methods[0])));

    int installed = 0;
    if (field_methods[0].fnPtr &&
        field_methods[0].fnPtr != reinterpret_cast<void *>(hook_get_declared_field)) {
        g_original_get_declared_field = reinterpret_cast<GetFieldByNameFn>(field_methods[0].fnPtr);
        ++installed;
    } else {
        log_error("field hook Class.getDeclaredField unavailable");
    }
    if (field_methods[1].fnPtr &&
        field_methods[1].fnPtr != reinterpret_cast<void *>(hook_get_public_field)) {
        g_original_get_public_field = reinterpret_cast<GetFieldByNameFn>(field_methods[1].fnPtr);
        ++installed;
    } else {
        log_error("field hook Class.getPublicFieldRecursive unavailable");
    }
    if (field_methods[2].fnPtr &&
        field_methods[2].fnPtr != reinterpret_cast<void *>(hook_get_declared_fields)) {
        g_original_get_declared_fields = reinterpret_cast<GetFieldsFn>(field_methods[2].fnPtr);
        ++installed;
    } else {
        log_error("field hook Class.getDeclaredFields unavailable");
    }
    if (field_methods[3].fnPtr &&
        field_methods[3].fnPtr != reinterpret_cast<void *>(hook_get_declared_fields0)) {
        g_original_get_declared_fields0 =
            reinterpret_cast<GetFieldsBoolFn>(field_methods[3].fnPtr);
        ++installed;
    } else {
        log_error("field hook Class.getDeclaredFields0 unavailable");
    }
    if (field_methods[4].fnPtr &&
        field_methods[4].fnPtr != reinterpret_cast<void *>(hook_get_declared_fields_unchecked)) {
        g_original_get_declared_fields_unchecked =
            reinterpret_cast<GetFieldsBoolFn>(field_methods[4].fnPtr);
        ++installed;
    } else {
        log_error("field hook Class.getDeclaredFieldsUnchecked unavailable");
    }

    if (installed == 0) return false;
    g_field_hooks_installed = true;
    log_info("LINEAGE_APK_PATH hidden from reflection (%d/5 hooks)", installed);
    return true;
}
