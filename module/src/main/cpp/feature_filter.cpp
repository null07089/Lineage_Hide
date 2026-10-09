#include "feature_filter.h"

#include <cstring>

namespace {
// Features declared by vendor/lineage/config/permissions/org.lineageos.*.xml.
// Names are exact and case-sensitive, matching SystemConfig parsing.
constexpr const char *kFeatures[] = {
    "org.lineageos.livedisplay",
    "org.lineageos.profiles",
    "org.lineageos.hardware",
    "org.lineageos.globalactions",
    "org.lineageos.trust",
    "org.lineageos.health",
    "org.lineageos.android",
    "org.lineageos.settings",
};

bool equals(std::string_view value, const char *needle) {
    if (!needle) return false;
    const std::size_t length = std::char_traits<char>::length(needle);
    return value.size() == length &&
           (length == 0 || std::memcmp(value.data(), needle, length) == 0);
}
} // namespace

bool hide_feature(std::string_view feature_name) {
    for (const char *feature : kFeatures) {
        if (equals(feature_name, feature)) return true;
    }
    return false;
}
