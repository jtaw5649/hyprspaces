#ifndef HYPRSPACES_RUNTIME_HPP
#define HYPRSPACES_RUNTIME_HPP

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/command.hpp"
#include "hyprspaces/config.hpp"
#include "hyprspaces/history.hpp"
#include "hyprspaces/hyprctl.hpp"
#include "hyprspaces/marks.hpp"
#include "hyprspaces/paths.hpp"
#include "hyprspaces/profiles.hpp"
#include "hyprspaces/workspace_switch.hpp"
#include "hyprspaces/workspace_rules.hpp"

namespace hyprspaces {

    enum class OutputFormat {
        kNormal,
        kJson,
    };

    struct RuntimeConfig {
        Paths             paths;
        Config            config;
        WorkspaceBindings bindings = {};
    };

    struct SetupActions {
        std::function<std::optional<std::string>(const std::vector<std::string>&)>                 apply_workspace_rules   = {};
        std::function<std::optional<std::string>()>                                                clear_workspace_rules   = {};
        std::function<std::optional<std::string>(const Paths&, const std::optional<std::string>&)> install_waybar_assets   = {};
        std::function<std::optional<std::string>(const Paths&)>                                    uninstall_waybar_assets = {};
    };

    struct CommandOutput {
        bool        success;
        std::string output;
    };

    using WorkspaceResolver = std::function<std::optional<int>(std::string_view, std::string* error)>;

    struct RuntimeStateProvider {
        std::function<std::optional<int>()>                        active_workspace_id   = {};
        std::function<std::optional<std::string>()>                active_window_address = {};
        std::function<std::optional<std::vector<MonitorInfo>>()>   monitors              = {};
        std::function<std::optional<std::vector<WorkspaceInfo>>()> workspaces            = {};
        std::function<std::optional<std::vector<ClientInfo>>()>    clients               = {};
    };

    struct StateSnapshot {
        int                        active_workspace_id;
        std::vector<MonitorInfo>   monitors;
        std::vector<WorkspaceInfo> workspaces;
        std::vector<ClientInfo>    clients;
    };

    std::expected<StateSnapshot, std::string> resolve_state_snapshot(const RuntimeStateProvider* provider, HyprctlClient& client);

    struct SessionCoordinator {
        std::function<std::optional<std::string>(std::string_view)> save    = {};
        std::function<std::optional<std::string>(std::string_view)> restore = {};
    };

    class RuntimeConfigCache {
      public:
        explicit RuntimeConfigCache(Paths paths);

        const RuntimeConfig& get(const ConfigOverrides& overrides, const WorkspaceBindings& bindings);

        void                 invalidate();

      private:
        Paths                          paths_;
        std::optional<ConfigOverrides> cached_overrides_;
        std::optional<RuntimeConfig>   cached_;
    };

    RuntimeConfig                load_runtime_config(const Paths& paths, const ConfigOverrides& overrides);

    std::optional<std::string>   ensure_workspace_rules(HyprctlClient& client, const RuntimeConfig& runtime_config, const SetupActions* setup_actions);
    std::optional<std::string>   ensure_cursor_no_warps(HyprctlClient& client);

    CommandOutput                run_hyprctl_command(HyprctlClient& client, const RuntimeConfig& runtime_config, OutputFormat format, std::string_view args,
                                                     const SetupActions* setup_actions = nullptr, const WorkspaceResolver& resolver = {}, WorkspaceHistory* history = nullptr,
                                                     const RuntimeStateProvider* state_provider = nullptr, const SessionCoordinator* session_coordinator = nullptr,
                                                     ProfileCatalog* profile_catalog = nullptr, const WorkspaceSwitchExecutor* executor = nullptr, MarksStore* marks = nullptr);

    CommandOutput                run_prefixed_command(HyprctlClient& client, const RuntimeConfig& runtime_config, std::string_view prefix, std::string_view args,
                                                      const WorkspaceResolver& resolver = {}, WorkspaceHistory* history = nullptr, const RuntimeStateProvider* state_provider = nullptr,
                                                      const SessionCoordinator* session_coordinator = nullptr, ProfileCatalog* profile_catalog = nullptr,
                                                      const WorkspaceSwitchExecutor* executor = nullptr, MarksStore* marks = nullptr);

    std::optional<CommandOutput> maybe_autosave(HyprctlClient& client, const RuntimeConfig& runtime_config, const RuntimeStateProvider* state_provider = nullptr,
                                                const SessionCoordinator* session_coordinator = nullptr);
    std::optional<CommandOutput> maybe_autorestore(HyprctlClient& client, const RuntimeConfig& runtime_config, const RuntimeStateProvider* state_provider = nullptr,
                                                   const SessionCoordinator* session_coordinator = nullptr);

} // namespace hyprspaces

#endif // HYPRSPACES_RUNTIME_HPP
