#include "hyprspaces/workspace_switch.hpp"

#include "hyprspaces/pairing.hpp"

namespace hyprspaces {

    std::optional<WorkspaceSwitchPlan> build_workspace_switch_plan(int requested_workspace, std::string_view focus_monitor, const Config& config, const WorkspaceBindings& bindings,
                                                                   std::string* error) {
        if (error) {
            error->clear();
        }
        if (config.workspace_count <= 0) {
            if (error) {
                *error = "workspace count must be positive";
            }
            return std::nullopt;
        }
        if (config.monitor_groups.empty()) {
            if (error) {
                *error = "monitor groups not configured";
            }
            return std::nullopt;
        }

        std::string resolved_focus;
        if (!focus_monitor.empty()) {
            resolved_focus = std::string(focus_monitor);
        }

        auto group_match = !resolved_focus.empty() ? find_group_for_monitor(resolved_focus, config.monitor_groups) : std::optional<MonitorGroupMatch>{};
        if (!group_match) {
            const auto by_workspace = find_group_for_workspace(requested_workspace, config.workspace_count, config.monitor_groups);
            if (by_workspace) {
                group_match = by_workspace;
                if (resolved_focus.empty() && by_workspace->group && by_workspace->monitor_index >= 0 &&
                    by_workspace->monitor_index < static_cast<int>(by_workspace->group->monitors.size())) {
                    resolved_focus = by_workspace->group->monitors[static_cast<size_t>(by_workspace->monitor_index)];
                }
            }
        }

        if (!group_match || !group_match->group) {
            if (error) {
                *error = "monitor group not found";
            }
            return std::nullopt;
        }

        if (resolved_focus.empty()) {
            resolved_focus = group_match->group->monitors.front();
        }

        const int           normalized = normalize_workspace(requested_workspace, config.workspace_count);

        WorkspaceSwitchPlan plan{
            .requested_workspace  = requested_workspace,
            .normalized_workspace = normalized,
            .group_name           = group_match->group->name,
            .focus_monitor        = resolved_focus,
            .targets              = {},
        };

        plan.targets.reserve(group_match->group->monitors.size());
        for (int i = 0; i < static_cast<int>(group_match->group->monitors.size()); ++i) {
            const auto workspace_id = workspace_id_for_group(*group_match->group, config.workspace_count, i, normalized);
            if (!workspace_id) {
                if (error) {
                    *error = "invalid workspace mapping";
                }
                return std::nullopt;
            }
            const auto monitor = monitor_for_workspace(*workspace_id, config, bindings);
            if (monitor.empty()) {
                if (error) {
                    *error = "missing monitor for workspace";
                }
                return std::nullopt;
            }
            plan.targets.push_back(MonitorWorkspaceTarget{.monitor = std::string(monitor), .workspace_id = *workspace_id});
        }

        return plan;
    }

} // namespace hyprspaces
