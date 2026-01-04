#ifndef HYPRSPACES_RUNTIME_UTILS_HPP
#define HYPRSPACES_RUNTIME_UTILS_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "hyprspaces/runtime.hpp"

namespace hyprspaces {

    std::string           build_prefixed_command(std::string_view prefix, std::string_view args);
    std::filesystem::path resolve_waybar_theme_path(const RuntimeConfig& runtime_config, const std::optional<std::string>& override_path);

} // namespace hyprspaces

#endif // HYPRSPACES_RUNTIME_UTILS_HPP
