#ifndef HYPRSPACES_WORKSPACE_RULES_HPP
#define HYPRSPACES_WORKSPACE_RULES_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "hyprspaces/config.hpp"
#include "hyprspaces/types.hpp"

namespace hyprspaces {

    struct WorkspaceRuleBinding {
        std::optional<int>         workspace_id;
        std::optional<std::string> workspace_name;
        std::string                monitor;
    };

    using WorkspaceBindings = std::unordered_map<int, std::string>;

    WorkspaceBindings resolve_workspace_bindings(const std::vector<WorkspaceRuleBinding>& rules, const std::vector<WorkspaceInfo>& workspaces);

    std::string_view  monitor_for_workspace(int workspace_id, const Config& config, const WorkspaceBindings& bindings);

} // namespace hyprspaces

#endif // HYPRSPACES_WORKSPACE_RULES_HPP
