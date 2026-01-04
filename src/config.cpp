#include "hyprspaces/config.hpp"

#include <string>

#include "hyprspaces/strings.hpp"

namespace hyprspaces {

    std::optional<SessionReason> parse_session_reason(std::string_view value) {
        if (value == "launch") {
            return SessionReason::kLaunch;
        }
        if (value == "recover") {
            return SessionReason::kRecover;
        }
        if (value == "session_restore") {
            return SessionReason::kSessionRestore;
        }
        return std::nullopt;
    }

    Config apply_overrides(const Config& base, const ConfigOverrides& overrides) {
        Config merged = base;
        if (overrides.monitor_groups) {
            merged.monitor_groups = *overrides.monitor_groups;
        }
        if (overrides.workspace_count) {
            merged.workspace_count = *overrides.workspace_count;
        }
        if (overrides.wrap_cycling) {
            merged.wrap_cycling = *overrides.wrap_cycling;
        }
        if (overrides.safe_mode) {
            merged.safe_mode = *overrides.safe_mode;
        }
        if (overrides.debug_logging) {
            merged.debug_logging = *overrides.debug_logging;
        }
        if (overrides.waybar_enabled) {
            merged.waybar_enabled = *overrides.waybar_enabled;
        }
        if (overrides.waybar_theme_css) {
            merged.waybar_theme_css = *overrides.waybar_theme_css;
        }
        if (overrides.waybar_class) {
            merged.waybar_class = *overrides.waybar_class;
        }
        if (overrides.session_enabled) {
            merged.session_enabled = *overrides.session_enabled;
        }
        if (overrides.session_autosave) {
            merged.session_autosave = *overrides.session_autosave;
        }
        if (overrides.session_autorestore) {
            merged.session_autorestore = *overrides.session_autorestore;
        }
        if (overrides.session_reason) {
            merged.session_reason = *overrides.session_reason;
        }
        if (overrides.persistent_all) {
            merged.persistent_all = *overrides.persistent_all;
        }
        if (overrides.persistent_workspaces) {
            merged.persistent_workspaces = *overrides.persistent_workspaces;
        }
        return merged;
    }

    std::optional<std::string> normalize_override_string(std::string_view value) {
        const auto trimmed = hyprspaces::trim_copy(value);
        if (trimmed.empty()) {
            return std::nullopt;
        }
        return trimmed;
    }

} // namespace hyprspaces
