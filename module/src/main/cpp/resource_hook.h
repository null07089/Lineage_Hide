#pragma once

#include <jni.h>

#include "zygisk.hpp"

// Hides the lineageos.platform resource package from the current process.
//
// LineageOS ships its platform SDK resources in
// /system/framework/org.lineageos.platform-res.apk using the reserved resource
// package id 0x3f.  Hooking AssetManager's resource lookups makes every
// name/ID lookup for that package behave as if the resources did not exist,
// matching what an application would see on a non-LineageOS device.  The hook
// is process-local by design: PackageManager replies, package lists, SDK
// classes and the /system files themselves are intentionally not touched.
//
// Call this in preAppSpecialize for target processes only.  It returns false
// when neither name nor value lookup could be hooked; partial hookups (for
// example, a changed signature on an odd ROM) still take effect and are
// reported in the module log.  The modern static AssetManager entry points
// exist since Android 9 (API 28); older releases keep their normal resource
// view.
bool install_resource_hook(JNIEnv *env, zygisk::Api *api);
