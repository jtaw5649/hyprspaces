#ifndef HYPRSPACES_ACTIONS_HPP
#define HYPRSPACES_ACTIONS_HPP

#include <string>

#include "hyprspaces/config.hpp"
#include "hyprspaces/history.hpp"
#include "hyprspaces/hyprctl.hpp"
#include "hyprspaces/pairing.hpp"
#include "hyprspaces/workspace_switch.hpp"
#include "hyprspaces/workspace_rules.hpp"

namespace hyprspaces {

    struct ActionResult {
        bool        success;
        std::string message;
    };

    ActionResult paired_switch(HyprctlClient& client, const Config& config, int workspace, const WorkspaceBindings& bindings = {}, const WorkspaceHistory* history = nullptr,
                               const WorkspaceSwitchExecutor* executor = nullptr);
    ActionResult paired_cycle(HyprctlClient& client, const Config& config, CycleDirection direction, const WorkspaceBindings& bindings = {},
                              const WorkspaceSwitchExecutor* executor = nullptr);
    ActionResult paired_move_window(HyprctlClient& client, const Config& config, int workspace, const WorkspaceBindings& bindings = {},
                                    const WorkspaceSwitchExecutor* executor = nullptr);
} // namespace hyprspaces

#endif // HYPRSPACES_ACTIONS_HPP
