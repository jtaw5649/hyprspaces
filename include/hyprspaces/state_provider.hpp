#ifndef HYPRSPACES_STATE_PROVIDER_HPP
#define HYPRSPACES_STATE_PROVIDER_HPP

#include <optional>
#include <string>
#include <vector>

#include "hyprspaces/events.hpp"
#include "hyprspaces/runtime.hpp"
#include "hyprspaces/workspace_rules.hpp"

namespace hyprspaces {

    std::optional<int>                        current_active_workspace_id();
    std::optional<std::vector<MonitorInfo>>   current_monitors();
    std::optional<std::vector<WorkspaceInfo>> current_workspaces();
    std::optional<std::vector<ClientInfo>>    current_clients();

    RuntimeStateProvider                      current_state_provider();
    WorkspaceBindings                         current_workspace_bindings();
    FocusResolvers                            focus_resolvers();
    WorkspaceResolver                         workspace_resolver();

} // namespace hyprspaces

#endif // HYPRSPACES_STATE_PROVIDER_HPP
