#ifndef HYPRSPACES_DISPATCH_HPP
#define HYPRSPACES_DISPATCH_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/config.hpp"
#include "hyprspaces/workspace_rules.hpp"
#include "hyprspaces/workspace_switch.hpp"

namespace hyprspaces {

    struct DispatchCommand {
        std::string dispatcher;
        std::string argument;
    };

    struct DispatchBatch {
        std::vector<DispatchCommand> commands;

        void                         dispatch(std::string dispatcher, std::string argument);
        std::string                  to_argument() const;
        bool                         empty() const;
    };

    std::string                  dispatch_batch(const std::vector<DispatchCommand>& commands);

    std::vector<DispatchCommand> group_switch_sequence(const std::vector<MonitorWorkspaceTarget>& targets, std::string_view focus_monitor);

    std::vector<DispatchCommand> rebalance_sequence(const Config& config, const WorkspaceBindings& bindings = {});

    std::optional<int>           move_window_target(int active_workspace, int requested_workspace, const Config& config);

    class DispatchGuard {
      public:
        DispatchGuard();
        DispatchGuard(const DispatchGuard&)            = delete;
        DispatchGuard& operator=(const DispatchGuard&) = delete;
        ~DispatchGuard();

        static bool active();

      private:
        bool previous_;
    };

    inline thread_local bool dispatch_guard_active = false;

    inline DispatchGuard::DispatchGuard() : previous_(dispatch_guard_active) {
        dispatch_guard_active = true;
    }

    inline DispatchGuard::~DispatchGuard() {
        dispatch_guard_active = previous_;
    }

    inline bool DispatchGuard::active() {
        return dispatch_guard_active;
    }

} // namespace hyprspaces

#endif // HYPRSPACES_DISPATCH_HPP
