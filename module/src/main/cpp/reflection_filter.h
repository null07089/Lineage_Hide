#pragma once

#include <jni.h>

#include "zygisk.hpp"

// Hides the LineageOS AssetManager.LINEAGE_APK_PATH constant from reflection
// inside the current process: getDeclaredField()/getField() behave as if the
// field did not exist and the getDeclaredFields() family omits it.  The class
// and the system image stay untouched.
// Call in preAppSpecialize for target processes only.  Returns false when no
// entry point could be hooked.
bool install_field_hooks(JNIEnv *env, zygisk::Api *api);
