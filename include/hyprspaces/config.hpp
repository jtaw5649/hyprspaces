#ifndef HYPRSPACES_CONFIG_HPP
#define HYPRSPACES_CONFIG_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/monitor_groups.hpp"

namespace hyprspaces {

    enum class SessionReason {
        kLaunch,
        kRecover,
        kSessionRestore,
    };

    struct Config {
        std::vector<MonitorGroup>  monitor_groups;
        int                        workspace_count;
        bool                       wrap_cycling;
        bool                       safe_mode;
        bool                       debug_logging  = false;
        bool                       waybar_enabled = true;
        std::optional<std::string> waybar_theme_css;
        std::optional<std::string> waybar_class;
        bool                       session_enabled       = false;
        bool                       session_autosave      = false;
        bool                       session_autorestore   = false;
        SessionReason              session_reason        = SessionReason::kSessionRestore;
        bool                       persistent_all        = true;
        std::vector<int>           persistent_workspaces = {};
    };

    struct ConfigOverrides {
        std::optional<std::vector<MonitorGroup>> monitor_groups;
        std::optional<int>                       workspace_count;
        std::optional<bool>                      wrap_cycling;
        std::optional<bool>                      safe_mode;
        std::optional<bool>                      debug_logging;
        std::optional<bool>                      waybar_enabled;
        std::optional<std::string>               waybar_theme_css;
        std::optional<std::string>               waybar_class;
        std::optional<bool>                      session_enabled;
        std::optional<bool>                      session_autosave;
        std::optional<bool>                      session_autorestore;
        std::optional<SessionReason>             session_reason = std::nullopt;
        std::optional<bool>                      persistent_all;
        std::optional<std::vector<int>>          persistent_workspaces;
    };

    std::optional<SessionReason> parse_session_reason(std::string_view value);
    Config                       apply_overrides(const Config& base, const ConfigOverrides& overrides);
    std::optional<std::string>   normalize_override_string(std::string_view value);

} // namespace hyprspaces

#endif // HYPRSPACES_CONFIG_HPP
