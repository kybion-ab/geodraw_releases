#pragma once

#include "geodraw/export/export.hpp"
#include <string>

namespace geodraw {

enum class LogLevel { ERROR = 0, WARNING = 1, INFO = 2, VERBOSE = 3, SPAM = 4 };

GEODRAW_API void set_log_level(LogLevel level);
GEODRAW_API LogLevel get_log_level();
GEODRAW_API void parse_log_args(int& argc, char** argv);

GEODRAW_API void log_error(const std::string& msg);
GEODRAW_API void log_warning(const std::string& msg);
GEODRAW_API void log_info(const std::string& msg);
GEODRAW_API void log_verbose(const std::string& msg);
GEODRAW_API void log_spam(const std::string& msg);

namespace detail {
GEODRAW_API void log_every_impl(LogLevel level, int interval_ms,
                                 const char* file, int line, const std::string& msg);
}

} // namespace geodraw

// Throttled logging macros (use call-site for unique key)
#define log_error_every(interval_ms, msg) \
    geodraw::detail::log_every_impl(geodraw::LogLevel::ERROR, interval_ms, __FILE__, __LINE__, msg)
#define log_warning_every(interval_ms, msg) \
    geodraw::detail::log_every_impl(geodraw::LogLevel::WARNING, interval_ms, __FILE__, __LINE__, msg)
#define log_info_every(interval_ms, msg) \
    geodraw::detail::log_every_impl(geodraw::LogLevel::INFO, interval_ms, __FILE__, __LINE__, msg)
#define log_verbose_every(interval_ms, msg) \
    geodraw::detail::log_every_impl(geodraw::LogLevel::VERBOSE, interval_ms, __FILE__, __LINE__, msg)
#define log_spam_every(interval_ms, msg) \
    geodraw::detail::log_every_impl(geodraw::LogLevel::SPAM, interval_ms, __FILE__, __LINE__, msg)
