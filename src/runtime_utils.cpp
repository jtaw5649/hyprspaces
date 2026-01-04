#include "hyprspaces/runtime_utils.hpp"

#include <cctype>

namespace hyprspaces {

    std::string build_prefixed_command(std::string_view prefix, std::string_view args) {
        std::string command(prefix);
        size_t      start = 0;
        while (start < args.size() && std::isspace(static_cast<unsigned char>(args[start]))) {
            ++start;
        }
        size_t end = args.size();
        while (end > start && std::isspace(static_cast<unsigned char>(args[end - 1]))) {
            --end;
        }
        if (end <= start) {
            return command;
        }
        if (!command.empty() && !std::isspace(static_cast<unsigned char>(command.back()))) {
            command.push_back(' ');
        }
        command.append(args.substr(start, end - start));
        return command;
    }

    std::filesystem::path resolve_waybar_theme_path(const RuntimeConfig& runtime_config, const std::optional<std::string>& override_path) {
        if (override_path) {
            return std::filesystem::path(*override_path);
        }
        if (runtime_config.config.waybar_theme_css) {
            return std::filesystem::path(*runtime_config.config.waybar_theme_css);
        }
        return runtime_config.paths.waybar_theme_css;
    }

} // namespace hyprspaces
