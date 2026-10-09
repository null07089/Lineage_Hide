#pragma once

#include <cstddef>
#include <string_view>

// Bounds of the hidden feature names.  The Parcel read hook uses them to skip
// unrelated strings before extracting their characters.
constexpr std::size_t kFeatureNameMinLength = 19;
constexpr std::size_t kFeatureNameMaxLength = 27;

bool hide_feature(std::string_view feature_name);
