#include "binder_hook.h"
#include "log.h"
#include "reflection_filter.h"
#include "resource_hook.h"
#include "service_cache.h"
#include "zygisk.hpp"

#include <jni.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
// Android assigns application processes uids starting at 10000; system
// components live below that.  Every third-party application process is
// injected, so the module needs no target list or configuration file.
constexpr jint kAppUidStart = 10000;

// Written at boot by service.sh: one system package per line.  When the list
// is present it is authoritative, so only third-party apps are injected.
constexpr const char *kSystemPackageList =
    "/data/adb/modules/lineage_hide/system_packages.txt";

std::array<char, 256> g_process_name{};
bool g_enabled_for_process = false;

std::string jstr_to_str(JNIEnv *env, jstring value) {
    if (!env || !value) return {};
    const char *raw = env->GetStringUTFChars(value, nullptr);
    if (!raw) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return {};
    }
    try {
        std::string out = raw;
        env->ReleaseStringUTFChars(value, raw);
        return out;
    } catch (...) {
        env->ReleaseStringUTFChars(value, raw);
        return {};
    }
}

std::string package_from_process_name(const std::string &name) {
    const std::size_t colon = name.find(':');
    return colon == std::string::npos ? name : name.substr(0, colon);
}

// Safety boundary for the boot window before service.sh has written the
// package list: never inject the ROM's own critical components.
bool is_fallback_system_component(const std::string &package) {
    constexpr const char *kExcludedPrefixes[] = {
        "android.process.",
        "com.android.systemui",
        "com.android.settings",
        "com.android.permissioncontroller",
        "com.android.providers.",
        "com.android.externalstorage",
        "com.android.documentsui",
        "com.android.phone",
        "com.android.server.telecom",
        "com.android.bluetooth",
        "com.android.nfc",
        "com.android.inputmethod.",
        "org.lineageos.",
    };
    for (const char *prefix : kExcludedPrefixes) {
        if (package.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

bool is_system_package(const std::string &package) {
    if (package.empty()) return true;
    FILE *fp = std::fopen(kSystemPackageList, "r");
    if (fp) {
        char line[256];
        bool list_has_entries = false;
        while (std::fgets(line, sizeof(line), fp)) {
            std::size_t length = std::strlen(line);
            while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
                line[--length] = '\0';
            }
            if (length == 0) continue;
            list_has_entries = true;
            if (package == line) {
                std::fclose(fp);
                return true;
            }
        }
        std::fclose(fp);
        if (list_has_entries) return false;
    }
    return is_fallback_system_component(package);
}
} // namespace

class LineageHideModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        api_ = api;
        env_ = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        g_enabled_for_process = false;
        resource_hook_ready_ = false;
        g_process_name.fill('\0');
        if (!args) return;

        if (args->uid < kAppUidStart) {
            if (api_) api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const std::string process_name = jstr_to_str(env_, args->nice_name);
        if (!process_name.empty()) {
            const size_t count = (process_name.size() < g_process_name.size() - 1)
                                     ? process_name.size()
                                     : g_process_name.size() - 1;
            std::memcpy(g_process_name.data(), process_name.data(), count);
            g_process_name[count] = '\0';
        }
        if (is_system_package(package_from_process_name(process_name))) {
            if (api_) api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        g_enabled_for_process = true;
        if (api_) api_->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
        set_feature_filtering(true);
        set_broadcast_filtering(true);
        // The Zygisk API is guaranteed to be live in preAppSpecialize.  Hook
        // the boot-class native methods here, before post-specialization API
        // calls become implementation-defined.
        try {
            install_jni_hook(env_, api_);
        } catch (...) {
            log_error("JNI hook setup failed; Binder filtering is skipped");
        }
        try {
            resource_hook_ready_ = install_resource_hook(env_, api_);
        } catch (...) {
            resource_hook_ready_ = false;
            log_error("resource hook setup failed; resources stay visible");
        }
        try {
            install_field_hooks(env_, api_);
        } catch (...) {
            log_error("field hook setup failed; LINEAGE_APK_PATH stays visible");
        }
        log_info("enabled uid=%d process=%s", args->uid, g_process_name.data());
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        if (!g_enabled_for_process) return;
        try {
            clear_cache(env_);
        } catch (...) {
            log_error("cache cleanup failed; continuing with binder instrumentation");
        }
        log_info("active for %s (resource hide=%d)", g_process_name.data(),
                 resource_hook_ready_ ? 1 : 0);
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *) override {
        // Lineage Hide is app-scoped. Do not leave the module resident in
        // system_server, where no filtering is needed and a native mapping
        // would only add observable surface.
        if (api_) api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    bool resource_hook_ready_ = false;
};

REGISTER_ZYGISK_MODULE(LineageHideModule)
