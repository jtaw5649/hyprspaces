#include "hyprspaces/registration.hpp"

namespace hyprspaces {

    std::vector<ConfigValueSpec> config_specs() {
        return {
            {"plugin:hyprspaces:workspace_count", ConfigValueType::kInt, 10},
            {"plugin:hyprspaces:wrap_cycling", ConfigValueType::kBool, true},
            {"plugin:hyprspaces:safe_mode", ConfigValueType::kBool, true},
            {"plugin:hyprspaces:debug_logging", ConfigValueType::kBool, false},
            {"plugin:hyprspaces:waybar_enabled", ConfigValueType::kBool, true},
            {"plugin:hyprspaces:waybar_theme_css", ConfigValueType::kString, std::string("")},
            {"plugin:hyprspaces:waybar_class", ConfigValueType::kString, std::string("")},
            {"plugin:hyprspaces:notify_session", ConfigValueType::kBool, true},
            {"plugin:hyprspaces:notify_timeout_ms", ConfigValueType::kInt, 3000},
            {"plugin:hyprspaces:session_enabled", ConfigValueType::kBool, false},
            {"plugin:hyprspaces:session_autosave", ConfigValueType::kBool, false},
            {"plugin:hyprspaces:session_autorestore", ConfigValueType::kBool, false},
            {"plugin:hyprspaces:session_reason", ConfigValueType::kString, std::string("session_restore")},
            {"plugin:hyprspaces:persistent_all", ConfigValueType::kBool, true},
        };
    }

} // namespace hyprspaces
