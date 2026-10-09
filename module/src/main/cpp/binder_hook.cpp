#include "binder_hook.h"
#include "broadcast_filter.h"
#include "feature_filter.h"
#include "log.h"
#include "service_cache.h"
#include "service_filter.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <linux/android/binder.h>
#include <pthread.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <vector>

#ifndef BC_TRANSACTION_SG
#define BC_TRANSACTION_SG _IOW('c', 17, struct binder_transaction_data_sg)
#endif

#ifndef TF_ONE_WAY
#define TF_ONE_WAY 0x01
#endif

#ifndef BC_FREE_BUFFER
#define BC_FREE_BUFFER _IOW('c', 3, binder_uintptr_t)
#endif

// Max reply buffer size for swap.  Keep the historical 256 KiB ceiling so
// large OEM service/debug enumerations are still filtered; allocation remains
// lazy and only occurs when a matching reply is received.
#define MAX_REPLY_BUF (256 * 1024)

namespace {
using IoctlFn = int (*)(int, unsigned long, void *);
IoctlFn g_original_ioctl = nullptr;
bool g_hook_installed = false;

using TransactNativeFn = jboolean (*)(JNIEnv *, jobject, jint, jobject, jobject, jint);
TransactNativeFn g_original_transact_native = nullptr;
bool g_jni_hook_installed = false;

constexpr const char *kIServiceManagerDescriptor = "android.os.IServiceManager";
constexpr const char *kIPackageManagerDescriptor = "android.content.pm.IPackageManager";
constexpr const char *kActivityManagerDescriptor = "android.app.IActivityManager";

constexpr jint kSvcListServices = 4;

struct ServiceTransactions {
    jint get_service = 0;
    jint check_service = 0;
    jint get_service2 = 0;
    jint check_service2 = 0;
    jint list_services = 0;
    jint debug_info = 0;
};

ServiceTransactions g_service_transactions;

// Feature hiding is opt-in per target process; when disabled the
// IPackageManager descriptor scan and Parcel read hooks are skipped.
struct PackageTransactions {
    jint has_system_feature = 0;
};

PackageTransactions g_package_transactions;

// Broadcast-action hiding is opt-in per target process.
struct ActivityTransactions {
    jint broadcast_intent = 0;
    jint broadcast_intent_with_feature = 0;
};

ActivityTransactions g_activity_transactions;
std::atomic<bool> g_feature_filtering{false};
std::atomic<bool> g_broadcast_filtering{false};

// Binder command buffers are bounded by the kernel's transaction limit.  Keep
// a conservative userspace ceiling before doing pointer arithmetic on data
// supplied by the driver.
constexpr binder_size_t kMaxCommandBytes = 4u * 1024u * 1024u;

struct ParcelMethods {
    jclass cls = nullptr;
    jmethodID data_size = nullptr;
    jmethodID data_position = nullptr;
    jmethodID set_data_position = nullptr;
    jmethodID read_string = nullptr;
    jmethodID write_string = nullptr;
    jmethodID read_string8 = nullptr;
    jmethodID write_string8 = nullptr;
    jmethodID read_int = nullptr;
    jmethodID obtain = nullptr;
    jmethodID append_from = nullptr;
    jmethodID recycle = nullptr;
};

struct CallerMethods {
    jclass thread_class = nullptr;
    jclass class_class = nullptr;
    jclass class_not_found = nullptr;
    jmethodID current_thread = nullptr;
    jmethodID get_stack_trace = nullptr;
    jmethodID get_class_name = nullptr;
    jmethodID for_name = nullptr;
};

CallerMethods g_caller_methods;
bool g_caller_methods_ready = false;

ParcelMethods g_parcel_methods;
pthread_mutex_t g_parcel_mutex = PTHREAD_MUTEX_INITIALIZER;
std::atomic<bool> g_parcel_methods_ready{false};
std::atomic<jint> g_sdk_int{-1};

// Thread local: whether the current thread has a pending synchronous
// transaction to handle 0 (servicemanager).
thread_local bool g_pending_sm_reply = false;

// --- Lazy Buffer Management (Optimized Memory Usage) ---
// Instead of allocating 256KB per thread upfront, we allocate on demand
// and clean up automatically when the thread exits using pthread_key.
thread_local unsigned char *g_swap_buf = nullptr; // Allocated on first use

// pthread_key destructor: automatically frees the buffer when thread exits
pthread_key_t g_buf_key;
pthread_once_t g_buf_key_once = PTHREAD_ONCE_INIT;
bool g_buf_key_ready = false;

void buf_destructor(void *buf) {
    if (buf) {
        free(buf);
    }
}

void buf_key_init() {
    g_buf_key_ready = pthread_key_create(&g_buf_key, buf_destructor) == 0;
}

// Returns a thread-local buffer, allocating it if necessary.
// Registered with pthread_key so it's freed on thread exit.
unsigned char *get_swap_buf() {
    if (!g_swap_buf) {
        if (pthread_once(&g_buf_key_once, buf_key_init) != 0 || !g_buf_key_ready) return nullptr;
        auto *buffer = static_cast<unsigned char *>(malloc(MAX_REPLY_BUF));
        if (!buffer) return nullptr;
        if (pthread_setspecific(g_buf_key, buffer) != 0) {
            free(buffer);
            return nullptr;
        }
        g_swap_buf = buffer;
    }
    return g_swap_buf;
}

// Track the active swap buffer address to intercept BC_FREE_BUFFER
thread_local void *g_active_swap_ptr = nullptr;
// The kernel-owned reply mapping must still be released.  Keep its pointer
// while the userspace replacement is visible to Java, then restore it in the
// BC_FREE_BUFFER command sent back to the driver.
thread_local binder_uintptr_t g_original_reply_ptr = 0;

struct FreeBufferPatch {
    uint8_t *slot = nullptr;
    binder_size_t consumed_end = 0;
    binder_uintptr_t swap_pointer = 0;
};

struct binder_transaction_data_sg_local {
    binder_transaction_data transaction_data;
    binder_size_t buffers_size;
};

struct ElfMappingId {
    dev_t dev = 0;
    ino_t inode = 0;
};

size_t align4(size_t value) { return (value + 3u) & ~static_cast<size_t>(3u); }
size_t align8(size_t value) { return (value + 7u) & ~static_cast<size_t>(7u); }

void clear_jni_exception(JNIEnv *env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

bool reset_reply_position(JNIEnv *env, jobject parcel) {
    clear_jni_exception(env);
    env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, 0);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    return true;
}

jint sdk_int(JNIEnv *env) {
    const jint cached = g_sdk_int.load(std::memory_order_acquire);
    if (cached >= 0) return cached;
    if (!env) return -1;

    jclass version = env->FindClass("android/os/Build$VERSION");
    if (!version) {
        clear_jni_exception(env);
        return -1;
    }
    const jfieldID field = env->GetStaticFieldID(version, "SDK_INT", "I");
    if (env->ExceptionCheck() || !field) {
        clear_jni_exception(env);
        env->DeleteLocalRef(version);
        return -1;
    }
    const jint value = env->GetStaticIntField(version, field);
    env->DeleteLocalRef(version);
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        return -1;
    }
    g_sdk_int.store(value, std::memory_order_release);
    return value;
}

bool is_list_transaction(JNIEnv *env, jint code) {
    (void)env;
    return g_service_transactions.list_services > 0 && code == g_service_transactions.list_services;
}

bool is_debug_transaction(JNIEnv *env, jint code) {
    (void)env;
    return g_service_transactions.debug_info > 0 && code == g_service_transactions.debug_info;
}

bool is_lookup_transaction(jint code) {
    return code > 0 && (code == g_service_transactions.get_service ||
                       code == g_service_transactions.check_service ||
                       code == g_service_transactions.get_service2 ||
                       code == g_service_transactions.check_service2);
}

jint read_transaction_field(JNIEnv *env, jclass stub, const char *name) {
    if (!env || !stub || !name) return 0;
    const jfieldID field = env->GetStaticFieldID(stub, name, "I");
    if (!field || env->ExceptionCheck()) {
        clear_jni_exception(env);
        return 0;
    }
    const jint value = env->GetStaticIntField(stub, field);
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        return 0;
    }
    return value > 0 ? value : 0;
}

void init_service_transactions(JNIEnv *env) {
    const jint sdk = sdk_int(env);
    ServiceTransactions transactions;
    if (sdk >= 26 && sdk <= 28) {
        transactions.get_service = 1;
        transactions.check_service = 2;
        transactions.list_services = kSvcListServices;
    }

    jclass stub = env ? env->FindClass("android/os/IServiceManager$Stub") : nullptr;
    if (stub) {
        transactions.get_service = read_transaction_field(env, stub, "TRANSACTION_getService");
        transactions.check_service = read_transaction_field(env, stub, "TRANSACTION_checkService");
        transactions.get_service2 = read_transaction_field(env, stub, "TRANSACTION_getService2");
        transactions.check_service2 = read_transaction_field(env, stub, "TRANSACTION_checkService2");
        transactions.list_services = read_transaction_field(env, stub, "TRANSACTION_listServices");
        transactions.debug_info = read_transaction_field(env, stub, "TRANSACTION_getServiceDebugInfo");
        env->DeleteLocalRef(stub);
    } else {
        clear_jni_exception(env);
    }
    g_service_transactions = transactions;

    jclass package_stub = env ? env->FindClass("android/content/pm/IPackageManager$Stub") : nullptr;
    if (package_stub) {
        PackageTransactions packages;
        packages.has_system_feature =
            read_transaction_field(env, package_stub, "TRANSACTION_hasSystemFeature");
        env->DeleteLocalRef(package_stub);
        g_package_transactions = packages;
    } else {
        clear_jni_exception(env);
    }

    jclass activity_stub = env ? env->FindClass("android/app/IActivityManager$Stub") : nullptr;
    if (activity_stub) {
        ActivityTransactions activities;
        activities.broadcast_intent =
            read_transaction_field(env, activity_stub, "TRANSACTION_broadcastIntent");
        activities.broadcast_intent_with_feature =
            read_transaction_field(env, activity_stub, "TRANSACTION_broadcastIntentWithFeature");
        env->DeleteLocalRef(activity_stub);
        g_activity_transactions = activities;
    } else {
        clear_jni_exception(env);
    }

    log_info("SM transactions: get=%d check=%d get2=%d check2=%d list=%d debug=%d; "
             "PM hasFeature=%d; AM broadcast=%d/%d",
             transactions.get_service, transactions.check_service, transactions.get_service2,
             transactions.check_service2, transactions.list_services, transactions.debug_info,
             g_package_transactions.has_system_feature,
             g_activity_transactions.broadcast_intent,
             g_activity_transactions.broadcast_intent_with_feature);
}

bool init_parcel_methods(JNIEnv *env) {
    if (!env) return false;
    if (g_parcel_methods_ready.load(std::memory_order_acquire)) return true;

    pthread_mutex_lock(&g_parcel_mutex);
    if (g_parcel_methods_ready.load(std::memory_order_relaxed)) {
        pthread_mutex_unlock(&g_parcel_mutex);
        return true;
    }

    jclass local = env->FindClass("android/os/Parcel");
    if (!local) {
        clear_jni_exception(env);
        pthread_mutex_unlock(&g_parcel_mutex);
        return false;
    }

    ParcelMethods methods;
    methods.cls = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    if (!methods.cls) {
        clear_jni_exception(env);
        pthread_mutex_unlock(&g_parcel_mutex);
        return false;
    }

    const auto find_method = [&](const char *name, const char *signature) -> jmethodID {
        if (env->ExceptionCheck()) return nullptr;
        return env->GetMethodID(methods.cls, name, signature);
    };
    methods.data_size = find_method("dataSize", "()I");
    methods.data_position = find_method("dataPosition", "()I");
    methods.set_data_position = find_method("setDataPosition", "(I)V");
    methods.read_string = find_method("readString", "()Ljava/lang/String;");
    methods.write_string = find_method("writeString", "(Ljava/lang/String;)V");
    methods.read_int = find_method("readInt", "()I");
    if (env->ExceptionCheck() || !methods.data_size || !methods.data_position ||
        !methods.set_data_position || !methods.read_string || !methods.write_string ||
        !methods.read_int) {
        clear_jni_exception(env);
        env->DeleteGlobalRef(methods.cls);
        pthread_mutex_unlock(&g_parcel_mutex);
        return false;
    }

    // Intent actions moved from writeString (UTF-16) to writeString8 (UTF-8);
    // both accessors are optional so older releases still get the UTF-16 path.
    methods.read_string8 = find_method("readString8", "()Ljava/lang/String;");
    if (env->ExceptionCheck() || !methods.read_string8) {
        clear_jni_exception(env);
        methods.read_string8 = nullptr;
    }
    if (methods.read_string8) {
        methods.write_string8 = find_method("writeString8", "(Ljava/lang/String;)V");
    }
    if (env->ExceptionCheck() || !methods.write_string8) {
        clear_jni_exception(env);
        methods.read_string8 = nullptr;
        methods.write_string8 = nullptr;
    }

    methods.obtain = env->GetStaticMethodID(methods.cls, "obtain", "()Landroid/os/Parcel;");
    if (!env->ExceptionCheck() && methods.obtain) {
        methods.append_from = find_method("appendFrom", "(Landroid/os/Parcel;II)V");
        methods.recycle = find_method("recycle", "()V");
    }
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        methods.obtain = nullptr;
        methods.append_from = nullptr;
        methods.recycle = nullptr;
    }

    g_parcel_methods = methods;
    g_parcel_methods_ready.store(true, std::memory_order_release);
    pthread_mutex_unlock(&g_parcel_mutex);
    return true;
}

std::string jstring_ascii(JNIEnv *env, jstring value) {
    if (!env || !value) return {};
    const jsize length = env->GetStringLength(value);
    if (env->ExceptionCheck() || length <= 0 || length > 512) return {};
    const jchar *chars = env->GetStringChars(value, nullptr);
    if (!chars) return {};
    try {
        std::string out;
        out.reserve(static_cast<size_t>(length));
        for (jsize i = 0; i < length; ++i) {
            const jchar c = chars[i];
            out.push_back(c <= 0x7f ? static_cast<char>(c) : '?');
        }
        env->ReleaseStringChars(value, chars);
        return out;
    } catch (...) {
        env->ReleaseStringChars(value, chars);
        return {};
    }
}

jstring replacement_for(JNIEnv *env, jstring value) {
    if (!env || !value) return nullptr;
    const jsize length = env->GetStringLength(value);
    if (env->ExceptionCheck() || length <= 0 || length > 512) return nullptr;
    try {
        std::u16string replacement(static_cast<size_t>(length), u'_');
        return env->NewString(reinterpret_cast<const jchar *>(replacement.data()), length);
    } catch (...) {
        return nullptr;
    }
}

jint interface_name_position(JNIEnv *env, jobject parcel, const char *descriptor) {
    if (!env || !parcel || !descriptor) return -1;
    const jint original_position = env->CallIntMethod(parcel, g_parcel_methods.data_position);
    if (env->ExceptionCheck() || original_position < 0) {
        clear_jni_exception(env);
        return -1;
    }
    const jint size = env->CallIntMethod(parcel, g_parcel_methods.data_size);
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        return -1;
    }
    // writeInterfaceToken prepends a kernel request header.  Its size changed
    // across releases: one int (O/P), two ints (Q), three ints (R), and four
    // ints on newer builds.  RPC parcels have no header and use offset zero.
    constexpr jint kCandidateOffsets[] = {0, 4, 8, 12, 16};
    const jint descriptor_length = static_cast<jint>(std::strlen(descriptor));
    const jint descriptor_bytes = 4 + ((2 * (descriptor_length + 1) + 3) & ~3);
    for (jint offset : kCandidateOffsets) {
        if (offset > size - descriptor_bytes) continue;
        env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, offset);
        if (env->ExceptionCheck()) {
            clear_jni_exception(env);
            break;
        }
        const jint length = env->CallIntMethod(parcel, g_parcel_methods.read_int);
        if (env->ExceptionCheck()) {
            clear_jni_exception(env);
            continue;
        }
        if (length != descriptor_length) continue;
        env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, offset);
        if (env->ExceptionCheck()) {
            clear_jni_exception(env);
            break;
        }
        jstring token = static_cast<jstring>(env->CallObjectMethod(parcel, g_parcel_methods.read_string));
        if (env->ExceptionCheck()) {
            if (token) env->DeleteLocalRef(token);
            clear_jni_exception(env);
            continue;
        }
        const std::string value = jstring_ascii(env, token);
        if (token) env->DeleteLocalRef(token);
        if (env->ExceptionCheck()) {
            clear_jni_exception(env);
            break;
        }
        if (value == descriptor) {
            const jint name_position = env->CallIntMethod(parcel, g_parcel_methods.data_position);
            const bool valid = !env->ExceptionCheck() && name_position >= 0 && name_position <= size;
            clear_jni_exception(env);
            env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, original_position);
            const bool restored = !env->ExceptionCheck();
            clear_jni_exception(env);
            return valid && restored ? name_position : -1;
        }
    }
    env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, original_position);
    clear_jni_exception(env);
    return -1;
}

bool init_caller_methods(JNIEnv *env) {
    if (g_caller_methods_ready) return true;
    if (!env || env->ExceptionCheck()) return false;
    if (env->PushLocalFrame(8) < 0) {
        clear_jni_exception(env);
        return false;
    }
    CallerMethods methods;
    jclass thread = env->FindClass("java/lang/Thread");
    jclass element = !env->ExceptionCheck() ? env->FindClass("java/lang/StackTraceElement") : nullptr;
    jclass class_class = !env->ExceptionCheck() ? env->FindClass("java/lang/Class") : nullptr;
    jclass class_not_found = !env->ExceptionCheck() ? env->FindClass("java/lang/ClassNotFoundException") : nullptr;
    if (thread && element && class_class && class_not_found && !env->ExceptionCheck()) {
        methods.current_thread = env->GetStaticMethodID(thread, "currentThread", "()Ljava/lang/Thread;");
        if (!env->ExceptionCheck()) {
            methods.get_stack_trace = env->GetMethodID(thread, "getStackTrace", "()[Ljava/lang/StackTraceElement;");
        }
        if (!env->ExceptionCheck()) {
            methods.get_class_name = env->GetMethodID(element, "getClassName", "()Ljava/lang/String;");
        }
        if (!env->ExceptionCheck()) {
            methods.for_name = env->GetStaticMethodID(class_class, "forName",
                                                     "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
        }
        if (!env->ExceptionCheck()) methods.thread_class = static_cast<jclass>(env->NewGlobalRef(thread));
        if (!env->ExceptionCheck()) methods.class_class = static_cast<jclass>(env->NewGlobalRef(class_class));
        if (!env->ExceptionCheck()) methods.class_not_found = static_cast<jclass>(env->NewGlobalRef(class_not_found));
    }
    const bool ready = !env->ExceptionCheck() && methods.current_thread && methods.get_stack_trace &&
                       methods.get_class_name && methods.for_name && methods.thread_class &&
                       methods.class_class && methods.class_not_found;
    clear_jni_exception(env);
    env->PopLocalFrame(nullptr);
    if (!ready) {
        if (methods.thread_class) env->DeleteGlobalRef(methods.thread_class);
        if (methods.class_class) env->DeleteGlobalRef(methods.class_class);
        if (methods.class_not_found) env->DeleteGlobalRef(methods.class_not_found);
        return false;
    }
    g_caller_methods = methods;
    g_caller_methods_ready = true;
    return true;
}

bool is_app_service_caller(JNIEnv *env) {
    if (!g_caller_methods_ready || !env || env->ExceptionCheck()) return false;
    if (env->PushLocalFrame(8) < 0) {
        clear_jni_exception(env);
        return false;
    }
    bool app_caller = false;
    jobject thread = env->CallStaticObjectMethod(g_caller_methods.thread_class, g_caller_methods.current_thread);
    auto stack = thread && !env->ExceptionCheck()
                     ? static_cast<jobjectArray>(env->CallObjectMethod(thread, g_caller_methods.get_stack_trace))
                     : nullptr;
    const jsize count = stack && !env->ExceptionCheck() ? env->GetArrayLength(stack) : 0;
    for (jsize index = 0; !env->ExceptionCheck() && index < count && index < 96; ++index) {
        jobject element = env->GetObjectArrayElement(stack, index);
        auto name = element && !env->ExceptionCheck()
                        ? static_cast<jstring>(env->CallObjectMethod(element, g_caller_methods.get_class_name))
                        : nullptr;
        const std::string class_name = !env->ExceptionCheck() ? jstring_ascii(env, name) : std::string();
        if (env->ExceptionCheck() || class_name.empty()) break;
        constexpr const char *kPlumbing[] = {
            "android.os.BinderProxy", "android.os.ServiceManager", "android.os.IServiceManager$",
            "java.lang.Thread", "java.lang.reflect.", "java.lang.invoke.", "jdk.internal.reflect.",
            "sun.reflect.", "libcore.reflect.", "dalvik.system.VMStack",
        };
        bool plumbing = false;
        for (const char *prefix : kPlumbing) {
            if (class_name.starts_with(prefix)) {
                plumbing = true;
                break;
            }
        }
        if (plumbing) {
            if (name) env->DeleteLocalRef(name);
            if (element) env->DeleteLocalRef(element);
            continue;
        }
        constexpr const char *kSystemPrefixes[] = {
            "android.", "com.android.", "java.", "javax.", "jdk.", "sun.", "libcore.",
            "dalvik.", "lineageos.", "org.lineageos.", "com.lineageos.",
        };
        bool framework = false;
        for (const char *prefix : kSystemPrefixes) {
            if (class_name.starts_with(prefix)) {
                framework = true;
                break;
            }
        }
        if (!framework) {
            jobject boot_class = env->CallStaticObjectMethod(g_caller_methods.class_class,
                                                            g_caller_methods.for_name, name, JNI_FALSE, nullptr);
            if (env->ExceptionCheck()) {
                jthrowable failure = env->ExceptionOccurred();
                env->ExceptionClear();
                app_caller = failure && env->IsInstanceOf(failure, g_caller_methods.class_not_found);
                if (failure) env->DeleteLocalRef(failure);
            }
            if (boot_class) env->DeleteLocalRef(boot_class);
        }
        break;
    }
    clear_jni_exception(env);
    env->PopLocalFrame(nullptr);
    return app_caller;
}

void recycle_request(JNIEnv *env, jobject parcel) {
    if (!parcel) return;
    jthrowable pending = env->ExceptionOccurred();
    if (pending) env->ExceptionClear();
    env->CallVoidMethod(parcel, g_parcel_methods.recycle);
    clear_jni_exception(env);
    env->DeleteLocalRef(parcel);
    if (pending) {
        env->Throw(pending);
        env->DeleteLocalRef(pending);
    }
}

using NamePredicate = bool (*)(const std::string &);

// Builds a length-preserving copy of the request Parcel with the name at
// name_position replaced by underscores when the predicate matches.  Service
// lookups additionally require an application caller so framework/Lineage
// initialization keeps the real service; package feature requests do not.
jobject filtered_name_request(JNIEnv *env, jobject parcel, jint name_position,
                              NamePredicate hide, bool require_app_caller) {
    if (!g_parcel_methods.obtain || !g_parcel_methods.append_from || !g_parcel_methods.recycle ||
        env->ExceptionCheck()) return nullptr;
    if (env->PushLocalFrame(8) < 0) {
        clear_jni_exception(env);
        return nullptr;
    }
    const jint original_position = env->CallIntMethod(parcel, g_parcel_methods.data_position);
    const jint size = !env->ExceptionCheck() ? env->CallIntMethod(parcel, g_parcel_methods.data_size) : -1;
    jstring name = nullptr;
    jint end = -1;
    if (!env->ExceptionCheck() && original_position >= 0 && size <= MAX_REPLY_BUF &&
        name_position >= 0 && name_position <= size - 4) {
        env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, name_position);
        if (!env->ExceptionCheck()) {
            name = static_cast<jstring>(env->CallObjectMethod(parcel, g_parcel_methods.read_string));
        }
        if (!env->ExceptionCheck()) end = env->CallIntMethod(parcel, g_parcel_methods.data_position);
    }
    const bool valid = !env->ExceptionCheck() && name && end >= name_position + 4 && end <= size;
    clear_jni_exception(env);
    if (original_position >= 0) env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, original_position);
    const std::string service = valid && !env->ExceptionCheck() ? jstring_ascii(env, name) : std::string();
    jobject copy = nullptr;
    if (!env->ExceptionCheck() && !service.empty() && hide(service) &&
        (!require_app_caller || is_app_service_caller(env))) {
        jstring replacement = replacement_for(env, name);
        if (replacement && !env->ExceptionCheck()) {
            copy = env->CallStaticObjectMethod(g_parcel_methods.cls, g_parcel_methods.obtain);
        }
        if (copy && !env->ExceptionCheck()) env->CallVoidMethod(copy, g_parcel_methods.append_from, parcel, 0, size);
        if (copy && !env->ExceptionCheck()) env->CallVoidMethod(copy, g_parcel_methods.set_data_position, name_position);
        if (copy && !env->ExceptionCheck()) env->CallVoidMethod(copy, g_parcel_methods.write_string, replacement);
        const jint copy_end = copy && !env->ExceptionCheck() ? env->CallIntMethod(copy, g_parcel_methods.data_position) : -1;
        const jint copy_size = copy && !env->ExceptionCheck() ? env->CallIntMethod(copy, g_parcel_methods.data_size) : -1;
        if (copy && !env->ExceptionCheck() && copy_end == end && copy_size == size) {
            env->CallVoidMethod(copy, g_parcel_methods.set_data_position, original_position);
        } else if (copy) {
            clear_jni_exception(env);
            recycle_request(env, copy);
            copy = nullptr;
        }
    }
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        if (copy) recycle_request(env, copy);
        copy = nullptr;
    }
    jobject retained = env->PopLocalFrame(copy);
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        if (retained) recycle_request(env, retained);
        return nullptr;
    }
    return retained;
}

// Broadcast requests embed the Intent early in the parcel; the action is a
// length-prefixed string (UTF-8 on newer releases, UTF-16 before that).
// ActivityManager rejects lineage* protected actions from apps, so scan a
// bounded prefix of a private copy and replace any protected action with an
// equal-length placeholder; the broadcast is then accepted as an ordinary,
// unprotected one.  The copy is owned by this process, so writing is safe.
constexpr jint kMaxBroadcastScanBytes = 8 * 1024;
constexpr jint kMinBroadcastActionLength = 32;
constexpr jint kMaxBroadcastActionLength = 80;

bool scrub_broadcast_action_at(JNIEnv *env, jobject parcel, jint position, jmethodID read,
                               jmethodID write) {
    env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, position);
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        return false;
    }
    auto value = static_cast<jstring>(env->CallObjectMethod(parcel, read));
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        if (value) env->DeleteLocalRef(value);
        return false;
    }
    if (!value) return false;
    const std::string action = jstring_ascii(env, value);
    jstring replacement = !action.empty() && hide_broadcast_action(action)
                              ? replacement_for(env, value)
                              : nullptr;
    env->DeleteLocalRef(value);
    if (!replacement || env->ExceptionCheck()) {
        if (replacement) env->DeleteLocalRef(replacement);
        clear_jni_exception(env);
        return false;
    }
    env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, position);
    if (!env->ExceptionCheck()) {
        env->CallVoidMethod(parcel, write, replacement);
    }
    env->DeleteLocalRef(replacement);
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        return false;
    }
    return true;
}

jobject filtered_broadcast_request(JNIEnv *env, jobject parcel) {
    if (!g_parcel_methods.obtain || !g_parcel_methods.append_from || !g_parcel_methods.recycle ||
        env->ExceptionCheck()) return nullptr;
    const jint size = env->CallIntMethod(parcel, g_parcel_methods.data_size);
    if (env->ExceptionCheck() || size <= 0 || size > MAX_REPLY_BUF) {
        clear_jni_exception(env);
        return nullptr;
    }
    if (env->PushLocalFrame(4) < 0) {
        clear_jni_exception(env);
        return nullptr;
    }
    jobject copy = env->CallStaticObjectMethod(g_parcel_methods.cls, g_parcel_methods.obtain);
    if (copy && !env->ExceptionCheck()) {
        env->CallVoidMethod(copy, g_parcel_methods.append_from, parcel, 0, size);
    }
    int hits = 0;
    const jint limit = size < kMaxBroadcastScanBytes ? size : kMaxBroadcastScanBytes;
    for (jint position = 0; copy && !env->ExceptionCheck() && position <= limit - 4;
         position += 4) {
        env->CallVoidMethod(copy, g_parcel_methods.set_data_position, position);
        if (env->ExceptionCheck()) {
            clear_jni_exception(env);
            break;
        }
        const jint length = env->CallIntMethod(copy, g_parcel_methods.read_int);
        if (env->ExceptionCheck()) {
            clear_jni_exception(env);
            continue;
        }
        if (length < kMinBroadcastActionLength || length > kMaxBroadcastActionLength) continue;
        if (g_parcel_methods.read_string8 &&
            scrub_broadcast_action_at(env, copy, position, g_parcel_methods.read_string8,
                                      g_parcel_methods.write_string8)) {
            ++hits;
            continue;
        }
        if (scrub_broadcast_action_at(env, copy, position, g_parcel_methods.read_string,
                                      g_parcel_methods.write_string)) {
            ++hits;
        }
    }
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        if (copy) recycle_request(env, copy);
        copy = nullptr;
    } else if (copy) {
        const jint copy_size = env->CallIntMethod(copy, g_parcel_methods.data_size);
        if (env->ExceptionCheck() || copy_size != size || hits == 0) {
            clear_jni_exception(env);
            recycle_request(env, copy);
            copy = nullptr;
        }
    }
    jobject retained = env->PopLocalFrame(copy);
    if (env->ExceptionCheck()) {
        clear_jni_exception(env);
        if (retained) recycle_request(env, retained);
        return nullptr;
    }
    if (retained && hits > 0) log_info("scrubbed %d lineage broadcast action(s)", hits);
    return retained;
}

bool filter_reply_string(JNIEnv *env, jobject parcel, jint limit) {
    const jint start = env->CallIntMethod(parcel, g_parcel_methods.data_position);
    if (env->ExceptionCheck() || start < 0 || start > limit - 4) return false;
    auto name = static_cast<jstring>(env->CallObjectMethod(parcel, g_parcel_methods.read_string));
    if (env->ExceptionCheck()) {
        if (name) env->DeleteLocalRef(name);
        return false;
    }
    const jint end = env->CallIntMethod(parcel, g_parcel_methods.data_position);
    if (env->ExceptionCheck() || end < start + 4 || end > limit) {
        if (name) env->DeleteLocalRef(name);
        return false;
    }
    if (!name) return true;
    const std::string service = jstring_ascii(env, name);
    if (!env->ExceptionCheck() && !service.empty() && hide_service(service)) {
        jstring replacement = replacement_for(env, name);
        if (replacement && !env->ExceptionCheck()) {
            env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, start);
            if (!env->ExceptionCheck()) {
                env->CallVoidMethod(parcel, g_parcel_methods.write_string, replacement);
            }
            if (!env->ExceptionCheck()) {
                env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, end);
            }
        }
        if (replacement) env->DeleteLocalRef(replacement);
    }
    env->DeleteLocalRef(name);
    return !env->ExceptionCheck();
}

void filter_list_reply(JNIEnv *env, jobject parcel) {
    if (!reset_reply_position(env, parcel)) return;
    const jint size = env->CallIntMethod(parcel, g_parcel_methods.data_size);
    if (env->ExceptionCheck() || size < 8 || size > MAX_REPLY_BUF) {
        reset_reply_position(env, parcel);
        return;
    }
    const jint sdk = g_sdk_int.load(std::memory_order_acquire);
    if (sdk >= 26 && sdk <= 28) {
        filter_reply_string(env, parcel, size);
        reset_reply_position(env, parcel);
        return;
    }
    const jint exception = env->CallIntMethod(parcel, g_parcel_methods.read_int);
    if (env->ExceptionCheck() || exception != 0) {
        reset_reply_position(env, parcel);
        return;
    }
    const jint count = env->CallIntMethod(parcel, g_parcel_methods.read_int);
    if (!env->ExceptionCheck() && count >= 0 && count <= (size - 8) / 4) {
        for (jint index = 0; index < count; ++index) {
            if (!filter_reply_string(env, parcel, size)) break;
        }
    }
    reset_reply_position(env, parcel);
}

void filter_debug_info_reply(JNIEnv *env, jobject parcel) {
    if (!reset_reply_position(env, parcel)) return;
    const jint size = env->CallIntMethod(parcel, g_parcel_methods.data_size);
    if (env->ExceptionCheck() || size < 8 || size > MAX_REPLY_BUF) {
        reset_reply_position(env, parcel);
        return;
    }
    const jint exception = env->CallIntMethod(parcel, g_parcel_methods.read_int);
    if (env->ExceptionCheck() || exception != 0) {
        reset_reply_position(env, parcel);
        return;
    }
    const jint count = env->CallIntMethod(parcel, g_parcel_methods.read_int);
    if (env->ExceptionCheck() || count < 0 || count > (size - 8) / 4) {
        reset_reply_position(env, parcel);
        return;
    }
    for (jint index = 0; index < count; ++index) {
        const jint position = env->CallIntMethod(parcel, g_parcel_methods.data_position);
        if (env->ExceptionCheck() || position < 0 || position > size - 4) break;
        const jint present = env->CallIntMethod(parcel, g_parcel_methods.read_int);
        if (env->ExceptionCheck()) break;
        if (present == 0) continue;
        if (present != 1) break;
        const jint object_start = env->CallIntMethod(parcel, g_parcel_methods.data_position);
        if (env->ExceptionCheck() || object_start < 0 || object_start > size - 4) break;
        const jint object_size = env->CallIntMethod(parcel, g_parcel_methods.read_int);
        if (env->ExceptionCheck() || object_size < 12 || object_size > size - object_start) break;
        const jint object_end = object_start + object_size;
        if (!filter_reply_string(env, parcel, object_end - 4)) break;
        (void)env->CallIntMethod(parcel, g_parcel_methods.read_int);
        if (env->ExceptionCheck()) break;
        env->CallVoidMethod(parcel, g_parcel_methods.set_data_position, object_end);
        if (env->ExceptionCheck()) break;
    }
    reset_reply_position(env, parcel);
}

// Feature hiding uses two interception points:
//  - hook_transact_native rewrites IPackageManager.hasSystemFeature requests,
//    so the framework answers false for the hidden names.
//  - Parcel's native string readers substitute an equal-length placeholder
//    when a hidden feature name is read, so getSystemAvailableFeatures and
//    other enumeration paths cannot observe the real names.  Only the returned
//    Java string is replaced; the Parcel bytes are never modified, so binder
//    reply buffers stay untouched.
using ParcelReadStringFn = jstring (*)(JNIEnv *, jclass, jlong);

ParcelReadStringFn g_original_parcel_read_string8 = nullptr;
ParcelReadStringFn g_original_parcel_read_string16 = nullptr;
bool g_feature_read_hooks_installed = false;

jstring filter_feature_read(JNIEnv *env, jstring value) {
    if (!env || !value || env->ExceptionCheck()) return value;
    const std::string name = jstring_ascii(env, value);
    if (name.empty() || !hide_feature(name)) return value;
    jstring replacement = replacement_for(env, value);
    if (!replacement || env->ExceptionCheck()) {
        if (replacement) env->DeleteLocalRef(replacement);
        clear_jni_exception(env);
        return value;
    }
    env->DeleteLocalRef(value);
    return replacement;
}

jstring hook_parcel_read_string8(JNIEnv *env, jclass clazz, jlong ptr) {
    if (!g_original_parcel_read_string8) return nullptr;
    return filter_feature_read(env, g_original_parcel_read_string8(env, clazz, ptr));
}

jstring hook_parcel_read_string16(JNIEnv *env, jclass clazz, jlong ptr) {
    if (!g_original_parcel_read_string16) return nullptr;
    return filter_feature_read(env, g_original_parcel_read_string16(env, clazz, ptr));
}

void install_feature_read_hooks(JNIEnv *env, zygisk::Api *api) {
    if (g_feature_read_hooks_installed || !env || !api) return;
    JNINativeMethod methods[] = {
        {"nativeReadString8", "(J)Ljava/lang/String;",
         reinterpret_cast<void *>(hook_parcel_read_string8)},
        {"nativeReadString16", "(J)Ljava/lang/String;",
         reinterpret_cast<void *>(hook_parcel_read_string16)},
    };
    api->hookJniNativeMethods(env, "android/os/Parcel", methods, 2);

    int installed = 0;
    if (methods[0].fnPtr && methods[0].fnPtr != reinterpret_cast<void *>(hook_parcel_read_string8)) {
        g_original_parcel_read_string8 = reinterpret_cast<ParcelReadStringFn>(methods[0].fnPtr);
        ++installed;
    }
    if (methods[1].fnPtr && methods[1].fnPtr != reinterpret_cast<void *>(hook_parcel_read_string16)) {
        g_original_parcel_read_string16 = reinterpret_cast<ParcelReadStringFn>(methods[1].fnPtr);
        ++installed;
    }
    if (installed == 0) {
        log_error("Parcel string read hook unavailable; feature enumeration stays visible");
        return;
    }
    g_feature_read_hooks_installed = true;
    log_info("Parcel string read hook installed (%d/2); lineage feature names filtered",
             installed);
}

jboolean hook_transact_native(JNIEnv *env, jobject thiz, jint code, jobject data_obj, jobject reply_obj,
                              jint flags);

std::string to_ascii(const char16_t *chars, int32_t len) {
    std::string ascii;
    if (!chars || len <= 0 || len > 512) return ascii;
    try {
        ascii.reserve(static_cast<size_t>(len));
        for (int32_t i = 0; i < len; ++i) {
            ascii.push_back(chars[i] <= 0x7f ? static_cast<char>(chars[i]) : '?');
        }
    } catch (...) {
        ascii.clear();
    }
    return ascii;
}

bool is_parcel_str16(const uint8_t *parcel, size_t size, size_t off, int32_t len) {
    if (!parcel || (off & 0x3u) != 0 || len <= 0 || len > 512) return false;
    const size_t str_off = off + sizeof(int32_t);
    const size_t bytes = static_cast<size_t>(len) * sizeof(char16_t);
    const size_t terminator_off = str_off + bytes;
    const size_t next_off = align4(terminator_off + sizeof(char16_t));
    if (next_off > size) return false;

    char16_t terminator = 1;
    std::memcpy(&terminator, parcel + terminator_off, sizeof(terminator));
    if (terminator != 0) return false;

    for (int32_t i = 0; i < len; ++i) {
        char16_t c = 0;
        std::memcpy(&c, parcel + str_off + static_cast<size_t>(i) * sizeof(char16_t), sizeof(c));
        if (c < 0x20 || c > 0x7e) return false;
    }
    return true;
}

void overwrite_utf16(char16_t *chars, int32_t len) {
    if (!chars || len <= 0) return;
    for (int32_t i = 0; i < len; ++i) chars[i] = u'_';
}

size_t process_string16(uint8_t *parcel, size_t size, size_t off, bool &hit) {
    if (off + sizeof(int32_t) > size) return 0;
    int32_t len = 0;
    std::memcpy(&len, parcel + off, sizeof(len));
    if (!is_parcel_str16(parcel, size, off, len)) return 0;

    const size_t str_off = off + sizeof(int32_t);
    const size_t bytes = static_cast<size_t>(len) * sizeof(char16_t);
    const size_t terminator_off = str_off + bytes;
    const size_t next_off = align4(terminator_off + sizeof(char16_t));

    auto *chars = reinterpret_cast<char16_t *>(parcel + str_off);
    const std::string value = to_ascii(chars, len);
    if (hide_service(value)) {
        overwrite_utf16(chars, len);
        hit = true;
        log_info("scrubbed service string in SM reply: %s", value.c_str());
    }
    return next_off - off;
}

// Scan a ServiceManager reply buffer for String16 values containing
// ROM keywords. Covers listServices, getServiceDebugInfo, etc.
void scan_sm_reply_for_strings(uint8_t *parcel, size_t size) {
    if (!parcel || size < sizeof(int32_t)) return;
    int total_hits = 0;

    for (size_t off = 0; off + sizeof(int32_t) <= size; off += sizeof(uint32_t)) {
        bool hit = false;
        process_string16(parcel, size, off, hit);
        if (hit) ++total_hits;
    }

    if (total_hits > 0) {
        log_info("scan_sm_reply: filtered %d service string(s)", total_hits);
    }
}

void process_transaction(const binder_transaction_data &txn) {
    // Ignore unrelated transactions.  libbinder can batch one-way writes
    // with a synchronous call; they must not cancel the pending SM reply.
    if (txn.target.handle != 0 || (txn.flags & TF_ONE_WAY) != 0) return;
    if (txn.data_size == 0 || txn.data.ptr.buffer == 0) {
        g_pending_sm_reply = false;
        return;
    }

    // Do not rewrite getService/checkService requests.  Returning a null
    // binder for a framework lookup can make callers crash during startup.
    // Enumeration/debug replies are scrubbed below, which is the stable and
    // low-risk observation boundary.
    const bool is_list = g_service_transactions.list_services > 0 &&
                         txn.code == static_cast<uint32_t>(g_service_transactions.list_services);
    const bool is_debug = g_service_transactions.debug_info > 0 &&
                          txn.code == static_cast<uint32_t>(g_service_transactions.debug_info);
    g_pending_sm_reply = is_list || is_debug;
}

// Copy reply to swap buffer, filter it, and replace the pointer.
bool swap_reply_buffer(binder_transaction_data *txn) {
    if (!txn || txn->data_size == 0 || txn->data.ptr.buffer == 0) return false;

    const binder_uintptr_t original_buffer = txn->data.ptr.buffer;
    const size_t data_size = static_cast<size_t>(txn->data_size);
    const size_t offsets_size = static_cast<size_t>(txn->offsets_size);
    if ((data_size & 0x3u) != 0 || (offsets_size & (sizeof(binder_size_t) - 1u)) != 0 ||
        data_size > kMaxCommandBytes || offsets_size > kMaxCommandBytes) {
        log_info("swap: malformed alignment (data=%zu offsets=%zu), skip", data_size, offsets_size);
        return false;
    }
    if (offsets_size > 0 && txn->data.ptr.offsets == 0) {
        log_info("swap: offsets_size without offsets pointer, skip");
        return false;
    }
    if (data_size > MAX_REPLY_BUF) {
        log_info("swap: reply data too large (%zu), skip", data_size);
        return false;
    }
    const size_t offsets_off = align8(data_size);
    if (offsets_off > MAX_REPLY_BUF || offsets_size > MAX_REPLY_BUF - offsets_off) {
        log_info("swap: reply too large (data=%zu offsets=%zu), skip", data_size, offsets_size);
        return false;
    }
    const size_t copy_size = offsets_off + offsets_size;

    if (copy_size > MAX_REPLY_BUF) {
        log_info("swap: reply too large (%zu), skip", copy_size);
        return false;
    }
    if (g_active_swap_ptr != nullptr) {
        log_info("swap: buffer still in use, skip");
        return false;
    }

    unsigned char *buf = get_swap_buf();
    if (!buf) {
        log_error("swap: failed to allocate swap buffer");
        return false;
    }

    std::memcpy(buf, reinterpret_cast<const void *>(original_buffer), data_size);
    if (offsets_size > 0 && txn->data.ptr.offsets != 0) {
        std::memcpy(buf + offsets_off, reinterpret_cast<const void *>(txn->data.ptr.offsets), offsets_size);
    }

    scan_sm_reply_for_strings(buf, data_size);

    txn->data.ptr.buffer = reinterpret_cast<binder_uintptr_t>(buf);
    if (offsets_size > 0 && txn->data.ptr.offsets != 0) {
        txn->data.ptr.offsets = reinterpret_cast<binder_uintptr_t>(buf + offsets_off);
    }

    g_active_swap_ptr = buf;
    g_original_reply_ptr = original_buffer;
    log_info("swap: replaced reply buffer with lazy swap (%zu bytes)", copy_size);
    return true;
}

void process_reply(binder_transaction_data *txn) {
    if (!g_pending_sm_reply) return;
    g_pending_sm_reply = false;

    if (!txn || txn->data_size == 0 || txn->data.ptr.buffer == 0) return;

    swap_reply_buffer(txn);
}

void process_write(binder_write_read *bwr, FreeBufferPatch &patch) {
    if (!bwr || !bwr->write_buffer || !bwr->write_size) return;
    if (bwr->write_size > kMaxCommandBytes || bwr->write_consumed > bwr->write_size) return;
    auto *begin = reinterpret_cast<uint8_t *>(bwr->write_buffer);
    auto *ptr = begin + bwr->write_consumed;
    auto *end = begin + bwr->write_size;

    while (static_cast<size_t>(end - ptr) >= sizeof(uint32_t)) {
        uint32_t cmd = 0;
        std::memcpy(&cmd, ptr, sizeof(cmd));
        ptr += sizeof(cmd);

        if (cmd == BC_TRANSACTION || cmd == BC_REPLY) {
            if (static_cast<size_t>(end - ptr) < sizeof(binder_transaction_data)) return;
            binder_transaction_data txn{};
            std::memcpy(&txn, ptr, sizeof(txn));
            if (cmd == BC_TRANSACTION) process_transaction(txn);
            ptr += sizeof(binder_transaction_data);
        } else if (cmd == BC_TRANSACTION_SG) {
            if (static_cast<size_t>(end - ptr) < sizeof(binder_transaction_data_sg_local)) return;
            binder_transaction_data_sg_local txn{};
            std::memcpy(&txn, ptr, sizeof(txn));
            process_transaction(txn.transaction_data);
            ptr += sizeof(binder_transaction_data_sg_local);
        } else if (cmd == BC_FREE_BUFFER) {
            if (static_cast<size_t>(end - ptr) < sizeof(binder_uintptr_t)) return;
            binder_uintptr_t buffer_pointer = 0;
            std::memcpy(&buffer_pointer, ptr, sizeof(buffer_pointer));
            if (g_active_swap_ptr != nullptr &&
                buffer_pointer == reinterpret_cast<binder_uintptr_t>(g_active_swap_ptr)) {
                patch.slot = ptr;
                patch.consumed_end = static_cast<binder_size_t>(ptr + sizeof(buffer_pointer) - begin);
                patch.swap_pointer = buffer_pointer;
                std::memcpy(ptr, &g_original_reply_ptr, sizeof(g_original_reply_ptr));
                log_info("swap: intercepted BC_FREE_BUFFER for swap buffer");
            }
            ptr += sizeof(binder_uintptr_t);
        } else {
            return;
        }
    }
}

void process_read(binder_write_read *bwr) {
    if (!bwr || !bwr->read_buffer || !bwr->read_consumed ||
        bwr->read_consumed > bwr->read_size || bwr->read_size > kMaxCommandBytes) return;
    auto *ptr = reinterpret_cast<uint8_t *>(bwr->read_buffer);
    auto *end = ptr + bwr->read_consumed;

    while (static_cast<size_t>(end - ptr) >= sizeof(uint32_t)) {
        uint32_t cmd = 0;
        std::memcpy(&cmd, ptr, sizeof(cmd));
        ptr += sizeof(cmd);

        switch (cmd) {
            case BR_REPLY: {
                if (static_cast<size_t>(end - ptr) < sizeof(binder_transaction_data)) return;
                binder_transaction_data txn{};
                std::memcpy(&txn, ptr, sizeof(txn));
                process_reply(&txn);
                std::memcpy(ptr, &txn, sizeof(txn));
                ptr += sizeof(binder_transaction_data);
                break;
            }
            case BR_TRANSACTION: {
                if (static_cast<size_t>(end - ptr) < sizeof(binder_transaction_data)) return;
                ptr += sizeof(binder_transaction_data);
                break;
            }
            case BR_DEAD_REPLY:
            case BR_FAILED_REPLY:
                g_pending_sm_reply = false;
                break;
            case BR_NOOP:
            case BR_TRANSACTION_COMPLETE:
            case BR_FINISHED:
                break;
            case BR_DEAD_BINDER:
            case BR_CLEAR_DEATH_NOTIFICATION_DONE:
                if (static_cast<size_t>(end - ptr) < sizeof(binder_uintptr_t)) return;
                ptr += sizeof(binder_uintptr_t);
                break;
            default:
                return;
        }
    }
}

jboolean hook_transact_native(JNIEnv *env, jobject thiz, jint code, jobject data_obj, jobject reply_obj,
                              jint flags) {
    (void)thiz;
    if (!g_original_transact_native) return JNI_FALSE;
    if (!env || env->ExceptionCheck()) {
        return g_original_transact_native(env, thiz, code, data_obj, reply_obj, flags);
    }

    const bool is_list = is_list_transaction(env, code);
    const bool is_debug = is_debug_transaction(env, code);
    const bool is_lookup = is_lookup_transaction(code) && (flags & TF_ONE_WAY) == 0;
    const bool feature_filtering = g_feature_filtering.load(std::memory_order_acquire);
    const bool is_has_feature = feature_filtering && g_package_transactions.has_system_feature > 0 &&
                                code == g_package_transactions.has_system_feature &&
                                (flags & TF_ONE_WAY) == 0;
    const bool broadcast_filtering = g_broadcast_filtering.load(std::memory_order_acquire);
    const bool is_broadcast = broadcast_filtering && (flags & TF_ONE_WAY) == 0 &&
                              ((g_activity_transactions.broadcast_intent > 0 &&
                                code == g_activity_transactions.broadcast_intent) ||
                               (g_activity_transactions.broadcast_intent_with_feature > 0 &&
                                code == g_activity_transactions.broadcast_intent_with_feature));
    jint name_position = -1;
    if ((is_list || is_debug || is_lookup || is_has_feature) && data_obj != nullptr &&
        init_parcel_methods(env)) {
        name_position = is_has_feature
                            ? interface_name_position(env, data_obj, kIPackageManagerDescriptor)
                            : interface_name_position(env, data_obj, kIServiceManagerDescriptor);
    }
    const bool descriptor_ready = name_position >= 0;

    jobject filtered_request = nullptr;
    if (descriptor_ready && is_lookup) {
        filtered_request = filtered_name_request(env, data_obj, name_position, hide_service, true);
    } else if (descriptor_ready && is_has_feature) {
        filtered_request = filtered_name_request(env, data_obj, name_position, hide_feature, false);
    }

    jobject broadcast_request = nullptr;
    if (is_broadcast && data_obj != nullptr && init_parcel_methods(env) &&
        interface_name_position(env, data_obj, kActivityManagerDescriptor) >= 0) {
        broadcast_request = filtered_broadcast_request(env, data_obj);
    }

    jobject request_to_send = filtered_request ? filtered_request : data_obj;
    if (broadcast_request) request_to_send = broadcast_request;
    const jboolean result = g_original_transact_native(env, thiz, code, request_to_send, reply_obj, flags);
    if (filtered_request) recycle_request(env, filtered_request);
    if (broadcast_request) recycle_request(env, broadcast_request);
    // Never swallow an exception raised by the real Binder implementation;
    // callers rely on RemoteException propagation semantics.
    if (result == JNI_FALSE || reply_obj == nullptr || env->ExceptionCheck()) {
        return result;
    }

    if (!init_parcel_methods(env)) return result;
    if (is_list && descriptor_ready) {
        filter_list_reply(env, reply_obj);
    } else if (is_debug && descriptor_ready) {
        filter_debug_info_reply(env, reply_obj);
    }
    clear_jni_exception(env);
    return result;
}

int hook_ioctl(int fd, unsigned long request, void *arg) {
    if (request != BINDER_WRITE_READ || !arg) {
        return g_original_ioctl ? g_original_ioctl(fd, request, arg) : -1;
    }

    auto *bwr = reinterpret_cast<binder_write_read *>(arg);
    FreeBufferPatch patch;
    process_write(bwr, patch);
    const int ret = g_original_ioctl ? g_original_ioctl(fd, request, arg) : -1;
    if (patch.slot) {
        if (ret == 0 && bwr->write_consumed >= patch.consumed_end) {
            g_active_swap_ptr = nullptr;
            g_original_reply_ptr = 0;
        } else {
            std::memcpy(patch.slot, &patch.swap_pointer, sizeof(patch.swap_pointer));
        }
    }
    if (ret == 0) process_read(bwr);
    return ret;
}

// A tiny RX-only trampoline makes the fallback PLT slot point at anonymous
// executable memory instead of the module .text mapping.  The trampoline
// tail-jumps to the real C++ callback, preserving all argument registers.
void *make_anonymous_thunk(void *target) {
    if (!target) return nullptr;
#if defined(__aarch64__)
    constexpr size_t kSize = 32;
    auto *code = static_cast<uint8_t *>(mmap(nullptr, kSize, PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (code == MAP_FAILED) return nullptr;
    const uint32_t insn[] = {0x58000050u, 0xD61F0200u}; // ldr x16, #8; br x16
    std::memcpy(code, insn, sizeof(insn));
    std::memcpy(code + 8, &target, sizeof(target));
#elif defined(__x86_64__)
    constexpr size_t kSize = 32;
    auto *code = static_cast<uint8_t *>(mmap(nullptr, kSize, PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (code == MAP_FAILED) return nullptr;
    // mov r11, imm64; jmp r11
    code[0] = 0x49;
    code[1] = 0xbb;
    std::memcpy(code + 2, &target, sizeof(target));
    code[10] = 0x41;
    code[11] = 0xff;
    code[12] = 0xe3;
#elif defined(__arm__)
    constexpr size_t kSize = 16;
    auto *code = static_cast<uint8_t *>(mmap(nullptr, kSize, PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (code == MAP_FAILED) return nullptr;
    // ldr pc, [pc, #-4]; followed by the absolute target address.
    const uint32_t insn = 0xe51ff004u;
    std::memcpy(code, &insn, sizeof(insn));
    std::memcpy(code + 4, &target, sizeof(target));
#else
    return target;
#endif
    __builtin___clear_cache(reinterpret_cast<char *>(code), reinterpret_cast<char *>(code + kSize));
    if (mprotect(code, kSize, PROT_READ | PROT_EXEC) != 0) {
        munmap(code, kSize);
        return nullptr;
    }
    return code;
}

bool seen(const std::vector<ElfMappingId> &mappings, dev_t dev, ino_t inode) {
    for (const auto &mapping : mappings) {
        if (mapping.dev == dev && mapping.inode == inode) return true;
    }
    return false;
}

std::vector<ElfMappingId> find_mappings() {
    std::vector<ElfMappingId> mappings;
    FILE *fp = std::fopen("/proc/self/maps", "r");
    if (!fp) return mappings;

    char line[1024]{};
    while (std::fgets(line, sizeof(line), fp)) {
        unsigned long long begin = 0, end = 0, offset = 0, inode = 0;
        unsigned int major_id = 0, minor_id = 0;
        char perms[5]{};
        char path[512]{};
        const int fields = std::sscanf(line, "%llx-%llx %4s %llx %x:%x %llu %511s", &begin, &end,
                                       perms, &offset, &major_id, &minor_id, &inode, path);
        if (fields < 8 || inode == 0) continue;
        const std::string pathname = path;
        if (pathname.find("/libbinder.so") == std::string::npos) continue;

        const dev_t dev = makedev(major_id, minor_id);
        const auto ino = static_cast<ino_t>(inode);
        if (!seen(mappings, dev, ino)) mappings.push_back({dev, ino});
    }
    std::fclose(fp);
    return mappings;
}
} // namespace

void set_feature_filtering(bool enabled) {
    g_feature_filtering.store(enabled, std::memory_order_release);
}

void set_broadcast_filtering(bool enabled) {
    g_broadcast_filtering.store(enabled, std::memory_order_release);
}

bool install_jni_hook(JNIEnv *env, zygisk::Api *api) {
    init_service_transactions(env);
    if (g_jni_hook_installed) return true;
    if (!env || !api || !init_parcel_methods(env)) {
        log_error("JNI BinderProxy hook unavailable: Parcel methods not found");
        return false;
    }
    if (!init_caller_methods(env)) {
        log_error("caller classification unavailable; direct service lookups left unchanged");
    }

    jclass binder_proxy = env->FindClass("android/os/BinderProxy");
    if (!binder_proxy) {
        clear_jni_exception(env);
        log_error("JNI BinderProxy hook unavailable: class not found");
        return false;
    }
    const jmethodID transact = env->GetMethodID(
        binder_proxy, "transactNative", "(ILandroid/os/Parcel;Landroid/os/Parcel;I)Z");
    if (env->ExceptionCheck() || !transact) {
        clear_jni_exception(env);
        env->DeleteLocalRef(binder_proxy);
        log_error("JNI BinderProxy hook unavailable: transactNative signature changed");
        return false;
    }

    JNINativeMethod method{"transactNative", "(ILandroid/os/Parcel;Landroid/os/Parcel;I)Z",
                           reinterpret_cast<void *>(hook_transact_native)};
    api->hookJniNativeMethods(env, "android/os/BinderProxy", &method, 1);
    env->DeleteLocalRef(binder_proxy);
    clear_jni_exception(env);

    auto original = reinterpret_cast<TransactNativeFn>(method.fnPtr);
    if (!original || original == hook_transact_native) {
        log_error("JNI BinderProxy hook did not return original function");
        return false;
    }
    g_original_transact_native = original;
    g_jni_hook_installed = true;
    if (g_feature_filtering.load(std::memory_order_acquire)) {
        install_feature_read_hooks(env, api);
    }
    log_info("BinderProxy.transactNative hook installed");
    return true;
}

void install_hooks(zygisk::Api *api) {
    if (g_hook_installed) return;
    if (!api) {
        log_error("zygisk api is null; cannot install binder hook");
        return;
    }

    const auto mappings = find_mappings();
    if (mappings.empty()) {
        log_error("libbinder mapping not found; cannot install ioctl hook");
        return;
    }
    g_original_ioctl = reinterpret_cast<IoctlFn>(dlsym(RTLD_DEFAULT, "ioctl"));
    if (!g_original_ioctl) {
        log_error("ioctl original unavailable; leaving Binder unchanged");
        return;
    }

    void *thunk = make_anonymous_thunk(reinterpret_cast<void *>(hook_ioctl));
    if (!thunk) {
        log_error("anonymous ioctl trampoline unavailable; leaving Binder unchanged");
        return;
    }
    for (const auto &mapping : mappings) {
        api->pltHookRegister(mapping.dev, mapping.inode, "ioctl", thunk,
                             reinterpret_cast<void **>(&g_original_ioctl));
    }
    if (!api->pltHookCommit() || !g_original_ioctl) {
        log_error("zygisk plt ioctl hook failed");
        return;
    }

    g_hook_installed = true;
    log_info("zygisk plt ioctl hook installed for %zu libbinder mapping(s)", mappings.size());
}
