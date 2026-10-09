#include "log.h"

#include <android/log.h>
#include <cstdarg>
#include <cstdio>

namespace {
constexpr const char *kTag = "LineageHide";

void vlog(int prio, const char *fmt, va_list ap) {
    if (!fmt) return;
    char buffer[1024]{};
    std::vsnprintf(buffer, sizeof(buffer), fmt, ap);
    __android_log_print(prio, kTag, "%s", buffer);
}
} // namespace

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(ANDROID_LOG_INFO, fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(ANDROID_LOG_ERROR, fmt, ap);
    va_end(ap);
}
