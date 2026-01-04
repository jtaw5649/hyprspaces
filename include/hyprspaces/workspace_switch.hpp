#ifndef HYPRSPACES_WORKSPACE_SWITCH_HPP
#define HYPRSPACES_WORKSPACE_SWITCH_HPP

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/config.hpp"
#include "hyprspaces/workspace_rules.hpp"

namespace hyprspaces {

    struct MonitorWorkspaceTarget {
        std::string monitor;
        int         workspace_id = 0;
    };

    struct WorkspaceSwitchPlan {
        int                                 requested_workspace  = 0;
        int                                 normalized_workspace = 0;
        std::string                         group_name;
        std::string                         focus_monitor;
        std::vector<MonitorWorkspaceTarget> targets;
    };

    std::optional<WorkspaceSwitchPlan> build_workspace_switch_plan(int requested_workspace, std::string_view focus_monitor, const Config& config, const WorkspaceBindings& bindings,
                                                                   std::string* error = nullptr);

    using WorkspaceSwitchExecutor = std::function<std::optional<std::string>(const WorkspaceSwitchPlan&)>;

} // namespace hyprspaces

#endif // HYPRSPACES_WORKSPACE_SWITCH_HPP
