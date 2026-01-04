#include "hyprspaces/dispatch.hpp"

#include "hyprspaces/pairing.hpp"

namespace hyprspaces {

    void DispatchBatch::dispatch(std::string dispatcher, std::string argument) {
        commands.push_back(DispatchCommand{std::move(dispatcher), std::move(argument)});
    }

    std::string DispatchBatch::to_argument() const {
        return dispatch_batch(commands);
    }

    bool DispatchBatch::empty() const {
        return commands.empty();
    }

    std::string dispatch_batch(const std::vector<DispatchCommand>& commands) {
        std::string output;
        output.reserve(commands.size() * 32);
        for (size_t i = 0; i < commands.size(); ++i) {
            if (i > 0) {
                output += " ; ";
            }
            output += "dispatch ";
            output += commands[i].dispatcher;
            output += " ";
            output += commands[i].argument;
        }
        return output;
    }

    std::vector<DispatchCommand> group_switch_sequence(const std::vector<MonitorWorkspaceTarget>& targets, std::string_view focus_monitor) {
        std::vector<DispatchCommand> commands;
        commands.reserve(targets.size() * 2u);

        if (targets.empty()) {
            return commands;
        }

        const auto emit_target = [&](const MonitorWorkspaceTarget& target) {
            commands.push_back({"focusmonitor", target.monitor});
            commands.push_back({"workspace", std::to_string(target.workspace_id)});
        };

        bool focus_found = false;
        if (!focus_monitor.empty()) {
            for (const auto& target : targets) {
                if (target.monitor == focus_monitor) {
                    focus_found = true;
                } else {
                    emit_target(target);
                }
            }
            if (focus_found) {
                for (const auto& target : targets) {
                    if (target.monitor == focus_monitor) {
                        emit_target(target);
                        break;
                    }
                }
                return commands;
            }
        }

        for (const auto& target : targets) {
            emit_target(target);
        }
        return commands;
    }

    std::vector<DispatchCommand> rebalance_sequence(const Config& config, const WorkspaceBindings& bindings) {
        std::vector<DispatchCommand> commands;
        if (config.workspace_count <= 0) {
            return commands;
        }
        for (const auto& group : config.monitor_groups) {
            if (group.monitors.empty()) {
                continue;
            }
            for (int index = 0; index < static_cast<int>(group.monitors.size()); ++index) {
                for (int slot = 1; slot <= config.workspace_count; ++slot) {
                    const auto workspace_id = workspace_id_for_group(group, config.workspace_count, index, slot);
                    if (!workspace_id) {
                        continue;
                    }
                    const auto monitor = monitor_for_workspace(*workspace_id, config, bindings);
                    if (monitor.empty()) {
                        continue;
                    }
                    commands.push_back({"moveworkspacetomonitor", std::to_string(*workspace_id) + " " + std::string(monitor)});
                }
            }
        }
        return commands;
    }

    std::optional<int> move_window_target(int active_workspace, int requested_workspace, const Config& config) {
        if (config.workspace_count <= 0) {
            return std::nullopt;
        }
        const auto match = find_group_for_workspace(active_workspace, config.workspace_count, config.monitor_groups);
        if (!match || !match->group) {
            return std::nullopt;
        }
        const int normalized = normalize_workspace(requested_workspace, config.workspace_count);
        return workspace_id_for_group(*match->group, config.workspace_count, match->monitor_index, normalized);
    }

} // namespace hyprspaces
