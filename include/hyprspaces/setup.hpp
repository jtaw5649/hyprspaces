#ifndef HYPRSPACES_SETUP_HPP
#define HYPRSPACES_SETUP_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hyprspaces/config.hpp"
#include "hyprspaces/paths.hpp"
#include "hyprspaces/types.hpp"

namespace hyprspaces {

    constexpr int kDefaultWorkspaceCount = 10;

    struct WorkspaceRuleSpec {
        int         workspace_id;
        std::string monitor;
        bool        is_default;
    };

    std::string                    render_hyprspaces_conf();
    bool                           ensure_hyprspaces_conf(const std::filesystem::path& config_path);
    bool                           ensure_hyprland_conf_source(const std::filesystem::path& hyprland_conf_path, const std::filesystem::path& include_path);
    bool                           ensure_hyprspaces_bootstrap(const Paths& paths);

    std::vector<WorkspaceRuleSpec> workspace_rules_for_config(const Config& config);

    std::vector<std::string>       workspace_rule_lines_for_config(const Config& config);

    std::optional<std::string>     install_waybar_assets(const Paths& paths, const std::optional<std::string>& theme_override);
    std::optional<std::string>     uninstall_waybar_assets(const Paths& paths);

} // namespace hyprspaces

#endif // HYPRSPACES_SETUP_HPP
