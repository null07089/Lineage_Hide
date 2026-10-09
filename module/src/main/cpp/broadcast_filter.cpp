#include "broadcast_filter.h"

#include <cstring>

namespace {
// Protected broadcasts declared in android_lineage-sdk/lineage/res/AndroidManifest.xml.
// Sending any of these from an app on LineageOS raises SecurityException while
// other ROMs accept it silently, which is a permissionless ROM fingerprint.
constexpr const char *kActions[] = {
    "lineageos.intent.action.SCREEN_CAMERA_GESTURE",
    "lineageos.intent.action.INITIALIZE_LINEAGE_HARDWARE",
    "lineageos.intent.action.INITIALIZE_LIVEDISPLAY",
    "lineageos.intent.action.UPDATE_PREFERENCE",
    "lineageos.intent.action.REFRESH_PREFERENCE",
    "lineageos.platform.intent.action.PROFILE_SELECTED",
    "lineageos.platform.intent.action.PROFILE_UPDATED",
    "lineageos.platform.intent.action.INTENT_ACTION_PROFILE_TRIGGER_STATE_CHANGED",
    "lineageos.platform.intent.action.UPDATE_TWILIGHT_STATE",
    "lineageos.platform.intent.action.CHARGING_CONTROL_CANCEL_ONCE",
};

bool equals(const std::string &value, const char *needle) {
    if (!needle) return false;
    const size_t length = std::char_traits<char>::length(needle);
    return value.size() == length && (length == 0 || std::memcmp(value.data(), needle, length) == 0);
}
} // namespace

bool hide_broadcast_action(const std::string &action) {
    for (const char *candidate : kActions) {
        if (equals(action, candidate)) return true;
    }
    return false;
}
