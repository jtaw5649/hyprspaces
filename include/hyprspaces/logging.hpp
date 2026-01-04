#ifndef HYPRSPACES_LOGGING_HPP
#define HYPRSPACES_LOGGING_HPP

#include <source_location>
#include <string>
#include <string_view>

namespace hyprspaces {

    using DebugLogSink = void (*)(std::string_view message);
    using ErrorLogSink = void (*)(std::string_view message);

    std::string format_log_entry(std::string_view context, std::string_view message);
    std::string format_debug_entry(std::string_view context, std::string_view message);
    std::string format_log_entry_with_location(std::string_view context, std::string_view message, const std::source_location& location = std::source_location::current());
    std::string format_debug_entry_with_location(std::string_view context, std::string_view message, const std::source_location& location = std::source_location::current());

    void        set_debug_log_sink(DebugLogSink sink);
    void        clear_debug_log_sink();
    void        debug_log(bool enabled, std::string_view context, std::string_view message, const std::source_location& location = std::source_location::current());

    void        set_error_log_sink(ErrorLogSink sink);
    void        clear_error_log_sink();
    void        error_log(std::string_view context, std::string_view message, const std::source_location& location = std::source_location::current());

} // namespace hyprspaces

#endif // HYPRSPACES_LOGGING_HPP
