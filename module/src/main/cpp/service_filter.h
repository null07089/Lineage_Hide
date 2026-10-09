#pragma once

#include <string_view>

// True when the service name contains a ROM keyword or matches the exact
// hidden service name.  The view overload lets JNI callers compare a raw
// UTF-8 buffer without allocating a std::string first.
bool hide_service(std::string_view service_name);
