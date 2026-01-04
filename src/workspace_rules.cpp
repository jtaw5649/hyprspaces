#include "hyprspaces/workspace_rules.hpp"

#include <algorithm>

namespace hyprspaces {

    namespace {

        std::string normalize_workspace_name(std::string_view name) {
            if (name.starts_with("name:")) {
                return std::string(name.substr(5));
            }
            return std::string(name);
        }

    } // namespace

    WorkspaceBindings resolve_workspace_bindings(const std::vector<WorkspaceRuleBinding>& rules, const std::vector<WorkspaceInfo>& workspaces) {
        WorkspaceBindings bindings;
        for (const auto& rule : rules) {
            if (rule.monitor.empty()) {
                continue;
            }
            if (rule.workspace_id && *rule.workspace_id > 0) {
                bindings[*rule.workspace_id] = rule.monitor;
                continue;
            }
            if (rule.workspace_name && !rule.workspace_name->empty()) {
                const auto target = normalize_workspace_name(*rule.workspace_name);
                auto       match  = std::find_if(workspaces.begin(), workspaces.end(), [&](const WorkspaceInfo& workspace) { return workspace.name && *workspace.name == target; });
                if (match != workspaces.end()) {
                    bindings[match->id] = rule.monitor;
                }
            }
        }
        return bindings;
    }

    std::string_view monitor_for_workspace(int workspace_id, const Config& config, const WorkspaceBindings& bindings) {
        if (const auto it = bindings.find(workspace_id); it != bindings.end()) {
            return it->second;
        }
        const auto match = find_group_for_workspace(workspace_id, config.workspace_count, config.monitor_groups);
        if (!match || !match->group) {
            return "";
        }
        const auto index = match->monitor_index;
        if (index < 0 || index >= static_cast<int>(match->group->monitors.size())) {
            return "";
        }
        return match->group->monitors[static_cast<size_t>(index)];
    }

} // namespace hyprspaces
