#include "hyprspaces/events.hpp"

#include <string>
#include <utility>

#include "hyprspaces/dispatch.hpp"
#include "hyprspaces/logging.hpp"
#include "hyprspaces/pairing.hpp"
#include "hyprspaces/workspace_switch.hpp"

namespace hyprspaces {

    namespace {

        std::optional<int> workspace_id_for_window(const std::string& address, const std::vector<ClientInfo>& clients) {
            for (const auto& client : clients) {
                if (client.address == address) {
                    return client.workspace.id;
                }
            }
            return std::nullopt;
        }

        std::optional<std::string> monitor_name_for_workspace(const std::vector<WorkspaceInfo>& workspaces, int workspace_id) {
            for (const auto& workspace : workspaces) {
                if (workspace.id == workspace_id) {
                    return workspace.monitor;
                }
            }
            return std::nullopt;
        }

        std::optional<int> workspace_id_for_monitor(const std::vector<WorkspaceInfo>& workspaces, std::string_view monitor) {
            for (const auto& workspace : workspaces) {
                if (workspace.monitor && *workspace.monitor == monitor) {
                    return workspace.id;
                }
            }
            return std::nullopt;
        }

    } // namespace

    bool focus_switch_for_event(HyprctlClient& client, const Config& config, const FocusEvent& event, FocusSwitchDebounce& debounce, const FocusResolvers& resolvers,
                                const WorkspaceBindings& bindings, WorkspaceHistory* history, const WorkspaceSwitchExecutor* executor) {
        if (DispatchGuard::active()) {
            return false;
        }
        if (event.window_address) {
            return false;
        }
        if (config.debug_logging) {
            std::string detail;
            detail.append("workspace_id=");
            if (event.workspace_id) {
                detail.append(std::to_string(*event.workspace_id));
            } else {
                detail.append("none");
            }
            detail.append(" window_address=");
            if (event.window_address) {
                detail.append(*event.window_address);
            } else {
                detail.append("none");
            }
            detail.append(" monitor_name=");
            if (event.monitor_name) {
                detail.append(*event.monitor_name);
            } else {
                detail.append("none");
            }
            debug_log(true, "focus event", detail);
        }
        std::optional<int> workspace_id = event.workspace_id;
        if (!workspace_id && event.window_address && resolvers.workspace_for_window) {
            workspace_id = resolvers.workspace_for_window(*event.window_address);
        }
        if (!workspace_id && event.window_address) {
            const auto clients = client.clients();
            if (!clients) {
                error_log(clients.error().context, clients.error().message);
                return false;
            }
            workspace_id = workspace_id_for_window(*event.window_address, *clients);
        }

        if (!workspace_id || *workspace_id <= 0) {
            return false;
        }

        const auto match = find_group_for_workspace(*workspace_id, config.workspace_count, config.monitor_groups);
        if (!match) {
            return false;
        }
        const int base_workspace = match->normalized_workspace;
        if (!debounce.should_switch(event.at, base_workspace)) {
            return false;
        }

        std::optional<std::string> focus_monitor = event.monitor_name;
        if (!focus_monitor && workspace_id && resolvers.monitor_for_workspace) {
            focus_monitor = resolvers.monitor_for_workspace(*workspace_id);
        }
        if (!focus_monitor) {
            const auto workspaces = client.workspaces();
            if (!workspaces) {
                error_log(workspaces.error().context, workspaces.error().message);
                return false;
            }
            focus_monitor = monitor_name_for_workspace(*workspaces, *workspace_id);
        }
        if (!focus_monitor || focus_monitor->empty()) {
            const auto fallback = monitor_for_workspace(*workspace_id, config, bindings);
            if (!fallback.empty()) {
                focus_monitor = std::string(fallback);
            }
        }
        if (!focus_monitor || focus_monitor->empty()) {
            return false;
        }
        const auto& resolved_monitor = *focus_monitor;
        if (history) {
            history->record(resolved_monitor, *workspace_id, config.workspace_count);
        }

        std::string error;
        const auto  plan = build_workspace_switch_plan(*workspace_id, resolved_monitor, config, bindings, &error);
        if (!plan) {
            return false;
        }

        const auto workspaces = client.workspaces();
        if (!workspaces) {
            error_log(workspaces.error().context, workspaces.error().message);
            return false;
        }
        bool matches = true;
        for (const auto& target : plan->targets) {
            const auto current_ws = workspace_id_for_monitor(*workspaces, target.monitor);
            if (!current_ws || *current_ws != target.workspace_id) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return false;
        }

        if (config.debug_logging) {
            std::string detail;
            detail.append("workspace_id=").append(std::to_string(plan->requested_workspace));
            detail.append(" normalized=").append(std::to_string(plan->normalized_workspace));
            detail.append(" resolved_monitor=").append(plan->focus_monitor);
            detail.append(" group=").append(plan->group_name);
            debug_log(true, "focus switch", detail);
        }
        if (executor) {
            DispatchGuard guard;
            const auto    error = (*executor)(*plan);
            return !error.has_value();
        }
        const auto commands = group_switch_sequence(plan->targets, plan->focus_monitor);
        if (config.debug_logging) {
            std::string target_detail;
            target_detail.append("group=").append(plan->group_name).append(" targets=");
            for (size_t i = 0; i < plan->targets.size(); ++i) {
                if (i != 0) {
                    target_detail.append(", ");
                }
                target_detail.append(plan->targets[i].monitor);
                target_detail.push_back(':');
                target_detail.append(std::to_string(plan->targets[i].workspace_id));
            }
            debug_log(true, "focus switch targets", target_detail);
        }
        DispatchGuard guard;
        const auto    output = client.dispatch_batch(commands);
        return is_ok_response(output);
    }

    bool rebalance_for_event(HyprctlClient& client, const Config& config, MonitorEventKind, RebalanceDebounce& debounce, std::chrono::steady_clock::time_point now,
                             const WorkspaceBindings& bindings) {
        const auto commands = rebalance_sequence(config, bindings);
        if (!debounce.record_event(now)) {
            return false;
        }
        const auto output = client.dispatch_batch(commands);
        return is_ok_response(output);
    }

    bool flush_pending_rebalance(HyprctlClient& client, const Config& config, RebalanceDebounce& debounce, std::chrono::steady_clock::time_point now,
                                 const WorkspaceBindings& bindings) {
        if (!debounce.flush(now)) {
            return false;
        }
        const auto commands = rebalance_sequence(config, bindings);
        const auto output   = client.dispatch_batch(commands);
        return is_ok_response(output);
    }

} // namespace hyprspaces
