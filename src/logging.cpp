#include "hyprspaces/logging.hpp"

#include <string>

namespace hyprspaces {

    namespace {

        DebugLogSink     debug_sink = nullptr;
        ErrorLogSink     error_sink = nullptr;

        std::string_view basename(std::string_view path) {
            const auto slash = path.find_last_of("/\\");
            if (slash == std::string_view::npos) {
                return path;
            }
            return path.substr(slash + 1);
        }

        std::string format_entry(std::string_view prefix, std::string_view context, std::string_view message) {
            std::string text;
            text.reserve(prefix.size() + context.size() + message.size() + 8);
            text.append(prefix);
            text.push_back(' ');
            if (!context.empty()) {
                text.append(context);
                text.append(": ");
            }
            text.append(message);
            return text;
        }

        std::string format_entry_with_location(std::string_view prefix, std::string_view context, std::string_view message, const std::source_location& location) {
            auto                   text = format_entry(prefix, context, message);
            const std::string_view file = basename(location.file_name());
            text.append(" @");
            text.append(file);
            text.push_back(':');
            text.append(std::to_string(location.line()));
            const std::string_view function = location.function_name();
            if (!function.empty()) {
                text.push_back(' ');
                text.append(function);
            }
            return text;
        }

    } // namespace

    std::string format_log_entry(std::string_view context, std::string_view message) {
        return format_entry("[hyprspaces]", context, message);
    }

    std::string format_debug_entry(std::string_view context, std::string_view message) {
        return format_entry("[hyprspaces][debug]", context, message);
    }

    std::string format_log_entry_with_location(std::string_view context, std::string_view message, const std::source_location& location) {
        return format_entry_with_location("[hyprspaces]", context, message, location);
    }

    std::string format_debug_entry_with_location(std::string_view context, std::string_view message, const std::source_location& location) {
        return format_entry_with_location("[hyprspaces][debug]", context, message, location);
    }

    void set_debug_log_sink(DebugLogSink sink) {
        debug_sink = sink;
    }

    void clear_debug_log_sink() {
        debug_sink = nullptr;
    }

    void debug_log(bool enabled, std::string_view context, std::string_view message, const std::source_location& location) {
        if (!enabled || !debug_sink) {
            return;
        }
        const auto text = format_debug_entry_with_location(context, message, location);
        debug_sink(text);
    }

    void set_error_log_sink(ErrorLogSink sink) {
        error_sink = sink;
    }

    void clear_error_log_sink() {
        error_sink = nullptr;
    }

    void error_log(std::string_view context, std::string_view message, const std::source_location& location) {
        if (!error_sink) {
            return;
        }
        const auto text = format_log_entry_with_location(context, message, location);
        error_sink(text);
    }

} // namespace hyprspaces
