#pragma once

#include <string>

// Returns true when the given system feature name belongs to the LineageOS
// platform and should be presented as nonexistent inside target processes.
bool hide_feature(const std::string &feature_name);
