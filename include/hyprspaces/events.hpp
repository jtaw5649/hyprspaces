#ifndef HYPRSPACES_EVENTS_HPP
#define HYPRSPACES_EVENTS_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>

#include "hyprspaces/config.hpp"
#include "hyprspaces/debounce.hpp"
#include "hyprspaces/history.hpp"
#include "hyprspaces/hyprctl.hpp"
#include "hyprspaces/workspace_switch.hpp"
#include "hyprspaces/workspace_rules.hpp"

namespace hyprspaces {

    enum class MonitorEventKind {
        kAdded,
        kRemoved,
    };

    struct FocusEvent {
        std::chrono::steady_clock::time_point at;
        std::optional<int>                    workspace_id;
        std::optional<std::string>            window_address;
        std::optional<std::string>            monitor_name;
    };

    struct FocusResolvers {
        std::function<std::optional<int>(std::string_view)> workspace_for_window;
        std::function<std::optional<std::string>(int)>      monitor_for_workspace;
    };

    bool focus_switch_for_event(HyprctlClient& client, const Config& config, const FocusEvent& event, FocusSwitchDebounce& debounce, const FocusResolvers& resolvers = {},
                                const WorkspaceBindings& bindings = {}, WorkspaceHistory* history = nullptr, const WorkspaceSwitchExecutor* executor = nullptr);

    bool rebalance_for_event(HyprctlClient& client, const Config& config, MonitorEventKind kind, RebalanceDebounce& debounce, std::chrono::steady_clock::time_point now,
                             const WorkspaceBindings& bindings = {});

    bool flush_pending_rebalance(HyprctlClient& client, const Config& config, RebalanceDebounce& debounce, std::chrono::steady_clock::time_point now,
                                 const WorkspaceBindings& bindings = {});

} // namespace hyprspaces

#endif // HYPRSPACES_EVENTS_HPP
