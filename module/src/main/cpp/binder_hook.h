#pragma once

#include <jni.h>

#include "zygisk.hpp"

// Opt-in filtering of LineageOS system features (IPackageManager
// hasSystemFeature / getSystemAvailableFeatures).  Call before installing the
// BinderProxy hook; it only affects the current process.
void set_feature_filtering(bool enabled);

// Opt-in filtering of LineageOS protected-broadcast actions in outbound
// IActivityManager broadcast requests.  Call before installing the
// BinderProxy hook; it only affects the current process.
void set_broadcast_filtering(bool enabled);

// Installs the preferred BinderProxy.transactNative hook. This path receives
// the framework-owned Java Parcel objects directly, does not take native
// ownership, and leaves libbinder's PLT/GOT relocation tables untouched. It
// returns false when the framework API is unavailable; callers may then use
// install_hooks() as a compatibility fallback.
bool install_jni_hook(JNIEnv *env, zygisk::Api *api);

// Legacy ioctl hook.  It is intentionally kept as a fallback for old Android
// releases which do not expose BinderProxy.transactNative through JNI.
void install_hooks(zygisk::Api *api);
