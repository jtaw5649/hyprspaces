#include "hyprspaces/actions.hpp"

#include <string>

#include "hyprspaces/dispatch.hpp"
#include "hyprspaces/logging.hpp"
#include "hyprspaces/workspace_switch.hpp"

namespace hyprspaces {

    namespace {

        ActionResult ok_result() {
            return ActionResult{true, "ok"};
        }

        ActionResult error_result(std::string_view message) {
            return ActionResult{false, std::string(message)};
        }

        std::string_view focus_monitor_for_workspace(int workspace_id, const Config& config, const WorkspaceBindings& bindings) {
            return monitor_for_workspace(workspace_id, config, bindings);
        }

        ActionResult apply_workspace_switch_plan(HyprctlClient& client, const WorkspaceSwitchPlan& plan, const WorkspaceSwitchExecutor* executor) {
            if (executor) {
                DispatchGuard guard;
                if (const auto error = (*executor)(plan)) {
                    return error_result(*error);
                }
                return ok_result();
            }
            const auto    commands = group_switch_sequence(plan.targets, plan.focus_monitor);
            DispatchGuard guard;
            const auto    output = client.dispatch_batch(commands);
            if (!is_ok_response(output)) {
                return error_result(output);
            }
            return ok_result();
        }

    } // namespace

    ActionResult paired_switch(HyprctlClient& client, const Config& config, int workspace, const WorkspaceBindings& bindings, const WorkspaceHistory* history,
                               const WorkspaceSwitchExecutor* executor) {
        const auto active_workspace = client.active_workspace_id();
        if (!active_workspace) {
            error_log(active_workspace.error().context, active_workspace.error().message);
            return error_result(format_hyprctl_error(active_workspace.error()));
        }
        const auto focus_monitor    = focus_monitor_for_workspace(*active_workspace, config, bindings);
        int        target_workspace = workspace;
        if (history) {
            const int requested_base = normalize_workspace(workspace, config.workspace_count);
            if (const auto current = history->current(focus_monitor); current && *current == requested_base) {
                if (const auto previous = history->previous(focus_monitor)) {
                    target_workspace = *previous;
                }
            }
        }
        std::string error;
        const auto  plan = build_workspace_switch_plan(target_workspace, focus_monitor, config, bindings, &error);
        if (!plan) {
            return error_result(error.empty() ? "monitor group unavailable" : error);
        }
        const bool debug_logging = config.debug_logging;
        if (debug_logging) {
            std::string detail;
            detail.append("active=").append(std::to_string(*active_workspace));
            detail.append(" requested=").append(std::to_string(workspace));
            detail.append(" target=").append(std::to_string(target_workspace));
            detail.append(" normalized=").append(std::to_string(plan->normalized_workspace));
            detail.append(" focus_monitor=").append(plan->focus_monitor);
            detail.append(" group=").append(plan->group_name);
            debug_log(true, "paired switch", detail);
            if (!executor) {
                std::string target_detail;
                for (size_t i = 0; i < plan->targets.size(); ++i) {
                    if (i != 0) {
                        target_detail.append(", ");
                    }
                    target_detail.append(plan->targets[i].monitor);
                    target_detail.push_back(':');
                    target_detail.append(std::to_string(plan->targets[i].workspace_id));
                }
                debug_log(true, "paired switch targets", target_detail);
            }
        }
        return apply_workspace_switch_plan(client, *plan, executor);
    }

    ActionResult paired_cycle(HyprctlClient& client, const Config& config, CycleDirection direction, const WorkspaceBindings& bindings, const WorkspaceSwitchExecutor* executor) {
        const auto active_workspace = client.active_workspace_id();
        if (!active_workspace) {
            error_log(active_workspace.error().context, active_workspace.error().message);
            return error_result(format_hyprctl_error(active_workspace.error()));
        }
        const auto match = find_group_for_workspace(*active_workspace, config.workspace_count, config.monitor_groups);
        if (!match) {
            return error_result("monitor group unavailable");
        }
        const int  base          = match->normalized_workspace;
        const int  target        = cycle_target(base, config.workspace_count, direction, config.wrap_cycling);
        const auto focus_monitor = focus_monitor_for_workspace(*active_workspace, config, bindings);
        if (focus_monitor.empty()) {
            return error_result("missing focus monitor");
        }
        std::string error;
        const auto  plan = build_workspace_switch_plan(target, focus_monitor, config, bindings, &error);
        if (!plan) {
            return error_result(error.empty() ? "monitor group unavailable" : error);
        }
        return apply_workspace_switch_plan(client, *plan, executor);
    }

    ActionResult paired_move_window(HyprctlClient& client, const Config& config, int workspace, const WorkspaceBindings& bindings, const WorkspaceSwitchExecutor* executor) {
        const auto active_workspace = client.active_workspace_id();
        if (!active_workspace) {
            error_log(active_workspace.error().context, active_workspace.error().message);
            return error_result(format_hyprctl_error(active_workspace.error()));
        }
        const auto target = move_window_target(*active_workspace, workspace, config);
        if (!target) {
            return error_result("monitor group unavailable");
        }
        DispatchGuard guard;
        const auto    move_output = client.dispatch("movetoworkspacesilent", std::to_string(*target));
        if (!is_ok_response(move_output)) {
            return error_result(move_output);
        }
        const auto focus_monitor = focus_monitor_for_workspace(*active_workspace, config, bindings);
        if (focus_monitor.empty()) {
            return error_result("missing focus monitor");
        }
        std::string error;
        const auto  plan = build_workspace_switch_plan(workspace, focus_monitor, config, bindings, &error);
        if (!plan) {
            return error_result(error.empty() ? "monitor group unavailable" : error);
        }
        return apply_workspace_switch_plan(client, *plan, executor);
    }

} // namespace hyprspaces
