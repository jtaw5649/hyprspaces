#include "hyprspaces/runtime.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <system_error>
#include <unordered_set>

#include <unistd.h>

#include "hyprspaces/actions.hpp"
#include "hyprspaces/logging.hpp"
#include "hyprspaces/setup.hpp"
#include "hyprspaces/status.hpp"
#include "hyprspaces/process_utils.hpp"
#include "hyprspaces/runtime_utils.hpp"
#include "hyprspaces/waybar.hpp"
#include "hyprspaces/version.hpp"

namespace hyprspaces {

    namespace {

        std::optional<std::string> read_file_contents(const std::filesystem::path& path) {
            std::ifstream input(path);
            if (!input.good()) {
                return std::nullopt;
            }
            return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        }

        std::optional<int> parse_int(std::string_view value) {
            try {
                size_t index  = 0;
                int    parsed = std::stoi(std::string(value), &index, 10);
                if (index != value.size()) {
                    return std::nullopt;
                }
                return parsed;
            } catch (const std::exception&) { return std::nullopt; }
        }

        bool monitor_groups_equal(const std::optional<std::vector<MonitorGroup>>& left, const std::optional<std::vector<MonitorGroup>>& right) {
            if (left.has_value() != right.has_value()) {
                return false;
            }
            if (!left || !right) {
                return true;
            }
            if (left->size() != right->size()) {
                return false;
            }
            for (size_t i = 0; i < left->size(); ++i) {
                const auto& l = (*left)[i];
                const auto& r = (*right)[i];
                if (l.profile != r.profile || l.name != r.name || l.workspace_offset != r.workspace_offset || l.monitors != r.monitors) {
                    return false;
                }
            }
            return true;
        }

        bool overrides_equal(const ConfigOverrides& left, const ConfigOverrides& right) {
            return monitor_groups_equal(left.monitor_groups, right.monitor_groups) && left.workspace_count == right.workspace_count && left.wrap_cycling == right.wrap_cycling &&
                left.safe_mode == right.safe_mode && left.debug_logging == right.debug_logging && left.waybar_enabled == right.waybar_enabled &&
                left.waybar_theme_css == right.waybar_theme_css && left.waybar_class == right.waybar_class && left.session_enabled == right.session_enabled &&
                left.session_autosave == right.session_autosave && left.session_autorestore == right.session_autorestore && left.session_reason == right.session_reason &&
                left.persistent_all == right.persistent_all && left.persistent_workspaces == right.persistent_workspaces;
        }

        HyprctlResult<int> resolve_active_workspace_id(const RuntimeStateProvider* provider, HyprctlClient& client) {
            if (provider && provider->active_workspace_id) {
                if (const auto resolved = provider->active_workspace_id()) {
                    return *resolved;
                }
            }
            return client.active_workspace_id();
        }

        std::optional<std::string> resolve_active_window_address(const RuntimeStateProvider* provider, HyprctlClient& client) {
            if (provider && provider->active_window_address) {
                if (const auto resolved = provider->active_window_address()) {
                    return *resolved;
                }
            }
            const auto result = client.active_window_address();
            if (!result) {
                error_log(result.error().context, result.error().message);
                return std::nullopt;
            }
            return *result;
        }

        HyprctlResult<std::vector<WorkspaceInfo>> resolve_workspaces(const RuntimeStateProvider* provider, HyprctlClient& client) {
            if (provider && provider->workspaces) {
                if (const auto resolved = provider->workspaces()) {
                    return *resolved;
                }
            }
            return client.workspaces();
        }

        HyprctlResult<std::vector<MonitorInfo>> resolve_monitors(const RuntimeStateProvider* provider, HyprctlClient& client) {
            if (provider && provider->monitors) {
                if (const auto resolved = provider->monitors()) {
                    return *resolved;
                }
            }
            return client.monitors();
        }

        HyprctlResult<std::vector<ClientInfo>> resolve_clients(const RuntimeStateProvider* provider, HyprctlClient& client) {
            if (provider && provider->clients) {
                if (const auto resolved = provider->clients()) {
                    return *resolved;
                }
            }
            return client.clients();
        }

        constexpr std::string_view kDefaultSessionId = "default";

        bool                       is_special_workspace(const std::optional<std::string>& name) {
            return name && name->starts_with("special:");
        }

        ProfileDefinition build_profile_definition(std::string_view name, const std::vector<WorkspaceInfo>& workspaces, const std::vector<ClientInfo>& clients) {
            ProfileDefinition profile{
                .name        = std::string(name),
                .description = std::nullopt,
                .workspaces  = {},
                .windows     = {},
            };

            std::unordered_set<int> special_ids;
            for (const auto& workspace : workspaces) {
                if (is_special_workspace(workspace.name)) {
                    special_ids.insert(workspace.id);
                }
            }

            for (const auto& workspace : workspaces) {
                if (workspace.id <= 0 || special_ids.contains(workspace.id)) {
                    continue;
                }
                ProfileWorkspaceSpec spec{
                    .profile      = profile.name,
                    .workspace_id = workspace.id,
                    .name         = workspace.name,
                    .layout       = std::nullopt,
                };
                profile.workspaces.push_back(std::move(spec));
            }

            for (const auto& client : clients) {
                if (client.workspace.id <= 0 || special_ids.contains(client.workspace.id)) {
                    continue;
                }
                ProfileWindowSpec spec{
                    .profile       = profile.name,
                    .app_id        = client.app_id,
                    .initial_class = std::nullopt,
                    .title         = client.title,
                    .command       = client.command,
                    .workspace_id  = client.workspace.id,
                    .floating      = client.floating,
                };

                if (!spec.app_id) {
                    if (client.initial_class) {
                        spec.initial_class = client.initial_class;
                    } else if (client.class_name) {
                        spec.initial_class = client.class_name;
                    }
                }

                if (!spec.app_id && !spec.initial_class && !spec.title && !spec.command) {
                    continue;
                }
                profile.windows.push_back(std::move(spec));
            }

            return profile;
        }

        std::optional<std::string> write_profile_file(const Paths& paths, const ProfileDefinition& profile) {
            std::error_code ec;
            std::filesystem::create_directories(paths.profiles_dir, ec);
            if (ec) {
                return "failed to create profiles directory";
            }

            const auto    filename = paths.profiles_dir / ("profile." + profile.name + ".conf");
            const auto    tempname = filename.string() + ".tmp";
            std::ofstream output(tempname, std::ios::trunc);
            if (!output.good()) {
                return "failed to write profile file";
            }
            output << render_profile_text(profile);
            output.close();
            if (!output.good()) {
                return "failed to write profile file";
            }
            std::filesystem::rename(tempname, filename, ec);
            if (ec) {
                return "failed to finalize profile file";
            }
            return std::nullopt;
        }

        std::optional<std::string> dispatch_workspace_setup(HyprctlClient& client, const ProfileWorkspaceSpec& spec) {
            if (spec.workspace_id <= 0) {
                return std::nullopt;
            }
            const auto select_output = client.dispatch("workspace", std::to_string(spec.workspace_id));
            if (!is_ok_response(select_output)) {
                return select_output;
            }
            if (spec.name && !spec.name->empty()) {
                const auto rename_output = client.dispatch("renameworkspace", std::to_string(spec.workspace_id) + " " + *spec.name);
                if (!is_ok_response(rename_output)) {
                    return rename_output;
                }
            }
            return std::nullopt;
        }

        std::optional<std::string> dispatch_window_move(HyprctlClient& client, int workspace_id, std::string_view address) {
            if (workspace_id <= 0 || address.empty()) {
                return std::nullopt;
            }
            const auto output = client.dispatch("movetoworkspace", std::to_string(workspace_id) + ",address:" + std::string(address));
            if (!is_ok_response(output)) {
                return output;
            }
            return std::nullopt;
        }

        std::optional<std::string> dispatch_window_floating(HyprctlClient& client, bool floating, std::string_view address) {
            if (address.empty()) {
                return std::nullopt;
            }
            const auto output = client.dispatch(floating ? "setfloating" : "settiled", "address:" + std::string(address));
            if (!is_ok_response(output)) {
                return output;
            }
            return std::nullopt;
        }

        std::optional<std::string> dispatch_profile_exec(HyprctlClient& client, const ProfileWindowSpec& spec) {
            if (!spec.command || spec.command->empty()) {
                return std::nullopt;
            }
            std::vector<std::string> rules;
            if (spec.workspace_id && *spec.workspace_id > 0) {
                rules.push_back("workspace " + std::to_string(*spec.workspace_id) + " silent");
            }
            if (spec.floating && *spec.floating) {
                rules.push_back("float");
            }
            std::string command;
            if (!rules.empty()) {
                command.append("[");
                for (size_t i = 0; i < rules.size(); ++i) {
                    if (i != 0) {
                        command.append("; ");
                    }
                    command.append(rules[i]);
                }
                command.append("] ");
            }
            command.append(*spec.command);
            const auto output = client.dispatch("exec", command);
            if (!is_ok_response(output)) {
                return output;
            }
            return std::nullopt;
        }

        std::optional<std::string> apply_profile(const ProfileDefinition& profile, HyprctlClient& client, const RuntimeStateProvider* provider) {
            const auto clients = resolve_clients(provider, client);
            if (!clients) {
                error_log(clients.error().context, clients.error().message);
                return format_hyprctl_error(clients.error());
            }
            std::unordered_set<std::string> used_addresses;
            for (const auto& workspace : profile.workspaces) {
                if (const auto error = dispatch_workspace_setup(client, workspace)) {
                    return error;
                }
            }

            for (const auto& window : profile.windows) {
                auto match_address = std::optional<std::string>{};
                for (const auto& client_info : *clients) {
                    if (used_addresses.contains(client_info.address)) {
                        continue;
                    }
                    bool matches = false;
                    if (window.app_id && client_info.app_id && *window.app_id == *client_info.app_id) {
                        matches = true;
                    } else if (window.initial_class) {
                        if (client_info.initial_class && *window.initial_class == *client_info.initial_class) {
                            matches = true;
                        } else if (client_info.class_name && *window.initial_class == *client_info.class_name) {
                            matches = true;
                        }
                    } else if (window.title && client_info.title && *window.title == *client_info.title) {
                        matches = true;
                    }
                    if (matches) {
                        match_address = client_info.address;
                        break;
                    }
                }

                if (match_address) {
                    used_addresses.insert(*match_address);
                    if (window.workspace_id) {
                        if (const auto error = dispatch_window_move(client, *window.workspace_id, *match_address)) {
                            return error;
                        }
                    }
                    if (window.floating) {
                        if (const auto error = dispatch_window_floating(client, *window.floating, *match_address)) {
                            return error;
                        }
                    }
                    continue;
                }

                if (const auto error = dispatch_profile_exec(client, window)) {
                    return error;
                }
            }

            return std::nullopt;
        }

        std::optional<int> resolve_workspace_id(std::string_view spec, const WorkspaceResolver& resolver, std::string* error) {
            if (spec.empty()) {
                if (error) {
                    *error = "missing workspace id";
                }
                return std::nullopt;
            }
            if (resolver) {
                if (const auto resolved = resolver(spec, error)) {
                    return resolved;
                }
                if (error && !error->empty()) {
                    return std::nullopt;
                }
            }
            const auto parsed = parse_int(spec);
            if (!parsed) {
                if (error) {
                    *error = "invalid workspace id";
                }
                return std::nullopt;
            }
            if (*parsed <= 0) {
                if (error) {
                    *error = "workspace id must be positive";
                }
                return std::nullopt;
            }
            return parsed;
        }

    } // namespace

    std::expected<StateSnapshot, std::string> resolve_state_snapshot(const RuntimeStateProvider* provider, HyprctlClient& client) {
        const auto active_workspace = resolve_active_workspace_id(provider, client);
        if (!active_workspace) {
            error_log(active_workspace.error().context, active_workspace.error().message);
            return std::unexpected(std::string("state snapshot failed: ") + format_hyprctl_error(active_workspace.error()));
        }
        auto workspaces = resolve_workspaces(provider, client);
        if (!workspaces) {
            error_log(workspaces.error().context, workspaces.error().message);
            return std::unexpected(std::string("state snapshot failed: ") + format_hyprctl_error(workspaces.error()));
        }
        auto monitors = resolve_monitors(provider, client);
        if (!monitors) {
            error_log(monitors.error().context, monitors.error().message);
            return std::unexpected(std::string("state snapshot failed: ") + format_hyprctl_error(monitors.error()));
        }
        auto clients = resolve_clients(provider, client);
        if (!clients) {
            error_log(clients.error().context, clients.error().message);
            return std::unexpected(std::string("state snapshot failed: ") + format_hyprctl_error(clients.error()));
        }

        return StateSnapshot{
            .active_workspace_id = *active_workspace,
            .monitors            = std::move(*monitors),
            .workspaces          = std::move(*workspaces),
            .clients             = std::move(*clients),
        };
    }

    RuntimeConfigCache::RuntimeConfigCache(Paths paths) : paths_(std::move(paths)) {}

    const RuntimeConfig& RuntimeConfigCache::get(const ConfigOverrides& overrides, const WorkspaceBindings& bindings) {
        const bool should_reload = !cached_ || !cached_overrides_ || !overrides_equal(*cached_overrides_, overrides);

        if (should_reload) {
            RuntimeConfig loaded = load_runtime_config(paths_, overrides);
            loaded.bindings      = bindings;
            cached_              = std::move(loaded);
            cached_overrides_    = overrides;
        } else {
            cached_->bindings = bindings;
        }

        return *cached_;
    }

    void RuntimeConfigCache::invalidate() {
        cached_.reset();
        cached_overrides_.reset();
    }

    RuntimeConfig load_runtime_config(const Paths& paths, const ConfigOverrides& overrides) {
        const Config base{
            .monitor_groups        = {},
            .workspace_count       = kDefaultWorkspaceCount,
            .wrap_cycling          = true,
            .safe_mode             = true,
            .debug_logging         = false,
            .waybar_enabled        = true,
            .waybar_theme_css      = std::nullopt,
            .waybar_class          = std::nullopt,
            .session_enabled       = false,
            .session_autosave      = false,
            .session_autorestore   = false,
            .session_reason        = SessionReason::kSessionRestore,
            .persistent_all        = true,
            .persistent_workspaces = {},
        };
        return RuntimeConfig{
            .paths  = paths,
            .config = apply_overrides(base, overrides),
        };
    }

    std::optional<std::string> ensure_workspace_rules(HyprctlClient& client, const RuntimeConfig& runtime_config, const SetupActions* setup_actions) {
        if (!setup_actions || !setup_actions->apply_workspace_rules) {
            return std::string("setup install unavailable");
        }

        const auto rule_lines = workspace_rule_lines_for_config(runtime_config.config);
        if (const auto error = setup_actions->apply_workspace_rules(rule_lines)) {
            return error;
        }

        const auto commands = rebalance_sequence(runtime_config.config, runtime_config.bindings);
        const auto output   = client.dispatch_batch(commands);
        if (!is_ok_response(output)) {
            return output;
        }
        return std::nullopt;
    }

    std::optional<std::string> ensure_cursor_no_warps(HyprctlClient& client) {
        const auto output = client.keyword("cursor:no_warps", "true");
        if (!is_ok_response(output)) {
            return output;
        }
        return std::nullopt;
    }

    CommandOutput run_hyprctl_command(HyprctlClient& client, const RuntimeConfig& runtime_config, OutputFormat format, std::string_view args, const SetupActions* setup_actions,
                                      const WorkspaceResolver& resolver, WorkspaceHistory* history, const RuntimeStateProvider* state_provider,
                                      const SessionCoordinator* session_coordinator, ProfileCatalog* profile_catalog, const WorkspaceSwitchExecutor* executor, MarksStore* marks) {
        const auto parsed = parse_command(args);
        if (std::holds_alternative<ParseError>(parsed)) {
            const auto error = std::get<ParseError>(parsed);
            return CommandOutput{false, error.message};
        }

        const RuntimeStateProvider* effective_provider = runtime_config.config.safe_mode ? nullptr : state_provider;
        const auto                  command            = std::get<Command>(parsed);
        switch (command.kind) {
            case CommandKind::kStatus: {
                const auto active_workspace = resolve_active_workspace_id(effective_provider, client);
                if (!active_workspace) {
                    error_log(active_workspace.error().context, active_workspace.error().message);
                    return CommandOutput{false, format_hyprctl_error(active_workspace.error())};
                }
                const auto output =
                    format == OutputFormat::kJson ? render_status_json(runtime_config.config, *active_workspace) : render_status(runtime_config.config, *active_workspace);
                return CommandOutput{true, output};
            }
            case CommandKind::kPairedSwitch: {
                std::string error;
                const auto  workspace = resolve_workspace_id(command.workspace_spec.value_or(""), resolver, &error);
                if (!workspace) {
                    return CommandOutput{false, error.empty() ? "invalid workspace id" : error};
                }
                const auto result = paired_switch(client, runtime_config.config, *workspace, runtime_config.bindings, history, executor);
                return CommandOutput{result.success, result.message};
            }
            case CommandKind::kPairedCycle: {
                const auto result = paired_cycle(client, runtime_config.config, *command.direction, runtime_config.bindings, executor);
                return CommandOutput{result.success, result.message};
            }
            case CommandKind::kPairedMoveWindow: {
                std::string error;
                const auto  workspace = resolve_workspace_id(command.workspace_spec.value_or(""), resolver, &error);
                if (!workspace) {
                    return CommandOutput{false, error.empty() ? "invalid workspace id" : error};
                }
                const auto result = paired_move_window(client, runtime_config.config, *workspace, runtime_config.bindings, executor);
                return CommandOutput{result.success, result.message};
            }
            case CommandKind::kSetupInstall: {
                if (!setup_actions || !setup_actions->apply_workspace_rules) {
                    return CommandOutput{false, "setup install unavailable"};
                }
                const auto rule_lines = workspace_rule_lines_for_config(runtime_config.config);
                if (const auto error = setup_actions->apply_workspace_rules(rule_lines)) {
                    return CommandOutput{false, *error};
                }
                const auto commands         = rebalance_sequence(runtime_config.config, runtime_config.bindings);
                const auto rebalance_output = client.dispatch_batch(commands);
                if (!is_ok_response(rebalance_output)) {
                    return CommandOutput{false, rebalance_output};
                }
                if (command.setup_waybar) {
                    if (!setup_actions->install_waybar_assets) {
                        return CommandOutput{false, "setup install missing waybar assets"};
                    }
                    if (const auto error = setup_actions->install_waybar_assets(runtime_config.paths, runtime_config.config.waybar_theme_css)) {
                        return CommandOutput{false, *error};
                    }
                }
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kSetupUninstall: {
                if (!setup_actions || !setup_actions->clear_workspace_rules) {
                    return CommandOutput{false, "setup uninstall unavailable"};
                }
                if (const auto error = setup_actions->clear_workspace_rules()) {
                    return CommandOutput{false, *error};
                }
                if (setup_actions->uninstall_waybar_assets) {
                    if (const auto error = setup_actions->uninstall_waybar_assets(runtime_config.paths)) {
                        return CommandOutput{false, *error};
                    }
                }
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kSessionSave: {
                if (!runtime_config.config.session_enabled) {
                    return CommandOutput{false, "session management disabled"};
                }
                if (!session_coordinator || !session_coordinator->save) {
                    return CommandOutput{false, "session protocol unavailable"};
                }
                if (const auto error = session_coordinator->save(kDefaultSessionId)) {
                    return CommandOutput{false, *error};
                }
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kSessionRestore: {
                if (!runtime_config.config.session_enabled) {
                    return CommandOutput{false, "session management disabled"};
                }
                if (!session_coordinator || !session_coordinator->restore) {
                    return CommandOutput{false, "session protocol unavailable"};
                }
                if (const auto error = session_coordinator->restore(kDefaultSessionId)) {
                    return CommandOutput{false, *error};
                }
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kSessionProfileList: {
                if (!profile_catalog) {
                    return CommandOutput{false, "profile catalog unavailable"};
                }
                const auto names = profile_catalog->list_names();
                if (names.empty()) {
                    return CommandOutput{true, ""};
                }
                std::string output;
                for (size_t i = 0; i < names.size(); ++i) {
                    if (i != 0) {
                        output.push_back('\n');
                    }
                    output.append(names[i]);
                }
                return CommandOutput{true, output};
            }
            case CommandKind::kSessionProfileSave: {
                if (!runtime_config.config.session_enabled) {
                    return CommandOutput{false, "session management disabled"};
                }
                if (!profile_catalog) {
                    return CommandOutput{false, "profile catalog unavailable"};
                }
                if (!session_coordinator || !session_coordinator->save) {
                    return CommandOutput{false, "session protocol unavailable"};
                }
                const auto profile_name = command.profile_name.value_or("");
                if (profile_name.empty()) {
                    return CommandOutput{false, "missing profile name"};
                }
                const auto workspaces = resolve_workspaces(effective_provider, client);
                if (!workspaces) {
                    error_log(workspaces.error().context, workspaces.error().message);
                    return CommandOutput{false, format_hyprctl_error(workspaces.error())};
                }
                auto clients = resolve_clients(effective_provider, client);
                if (!clients) {
                    error_log(clients.error().context, clients.error().message);
                    return CommandOutput{false, format_hyprctl_error(clients.error())};
                }
                fill_client_commands(*clients, runtime_config.paths.proc_root);
                auto profile = build_profile_definition(profile_name, *workspaces, *clients);
                if (const auto existing = profile_catalog->find(profile.name); existing && existing->description) {
                    profile.description = existing->description;
                }
                if (const auto error = session_coordinator->save(profile.name)) {
                    return CommandOutput{false, *error};
                }
                if (const auto error = write_profile_file(runtime_config.paths, profile)) {
                    return CommandOutput{false, *error};
                }
                profile_catalog->upsert_profile(std::move(profile));
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kSessionProfileLoad: {
                if (!runtime_config.config.session_enabled) {
                    return CommandOutput{false, "session management disabled"};
                }
                if (!profile_catalog) {
                    return CommandOutput{false, "profile catalog unavailable"};
                }
                if (!session_coordinator || !session_coordinator->restore) {
                    return CommandOutput{false, "session protocol unavailable"};
                }
                const auto profile_name = command.profile_name.value_or("");
                if (profile_name.empty()) {
                    return CommandOutput{false, "missing profile name"};
                }
                const auto profile = profile_catalog->find(profile_name);
                if (!profile) {
                    return CommandOutput{false, "profile not found"};
                }
                if (const auto error = session_coordinator->restore(profile_name)) {
                    return CommandOutput{false, *error};
                }
                if (const auto error = apply_profile(*profile, client, effective_provider)) {
                    return CommandOutput{false, *error};
                }
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kMarkSet: {
                if (!marks) {
                    return CommandOutput{false, "marks unavailable"};
                }
                const auto name = command.mark_name.value_or("");
                if (name.empty()) {
                    return CommandOutput{false, "missing mark name"};
                }
                const auto address = resolve_active_window_address(effective_provider, client);
                if (!address) {
                    return CommandOutput{false, "active window unavailable"};
                }
                marks->set(std::string(name), *address);
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kMarkJump: {
                if (!marks) {
                    return CommandOutput{false, "marks unavailable"};
                }
                const auto name = command.mark_name.value_or("");
                if (name.empty()) {
                    return CommandOutput{false, "missing mark name"};
                }
                const auto address = marks->get(name);
                if (!address) {
                    return CommandOutput{false, "mark not found"};
                }
                const auto output = client.dispatch("focuswindow", "address:" + *address);
                if (!is_ok_response(output)) {
                    return CommandOutput{false, output};
                }
                return CommandOutput{true, "ok"};
            }
            case CommandKind::kWaybar: {
                const auto theme_path = resolve_waybar_theme_path(runtime_config, command.waybar_theme_css);
                const auto theme_css  = read_file_contents(theme_path);
                if (!theme_css) {
                    return CommandOutput{false, "missing waybar theme css"};
                }
                const auto theme = parse_waybar_theme(*theme_css);
                if (!theme) {
                    return CommandOutput{false, "invalid waybar theme css"};
                }
                try {
                    const auto snapshot = resolve_state_snapshot(effective_provider, client);
                    if (!snapshot) {
                        return CommandOutput{false, snapshot.error()};
                    }
                    const auto output =
                        render_waybar_json(runtime_config.config, snapshot->active_workspace_id, snapshot->workspaces, snapshot->monitors, snapshot->clients, *theme);
                    return CommandOutput{true, output};
                } catch (const std::exception& ex) { return CommandOutput{false, std::string("waybar render failed: ") + ex.what()}; }
            }
            default: return CommandOutput{false, "unknown command"};
        }
    }

    CommandOutput run_prefixed_command(HyprctlClient& client, const RuntimeConfig& runtime_config, std::string_view prefix, std::string_view args,
                                       const WorkspaceResolver& resolver, WorkspaceHistory* history, const RuntimeStateProvider* state_provider,
                                       const SessionCoordinator* session_coordinator, ProfileCatalog* profile_catalog, const WorkspaceSwitchExecutor* executor, MarksStore* marks) {
        const auto command = build_prefixed_command(prefix, args);
        return run_hyprctl_command(client, runtime_config, OutputFormat::kNormal, command, nullptr, resolver, history, state_provider, session_coordinator, profile_catalog,
                                   executor, marks);
    }

    std::optional<CommandOutput> maybe_autosave(HyprctlClient& client, const RuntimeConfig& runtime_config, const RuntimeStateProvider* state_provider,
                                                const SessionCoordinator* session_coordinator) {
        if (!runtime_config.config.session_autosave) {
            return std::nullopt;
        }
        return run_hyprctl_command(client, runtime_config, OutputFormat::kNormal, "session save", nullptr, {}, nullptr, state_provider, session_coordinator, nullptr, nullptr,
                                   nullptr);
    }

    std::optional<CommandOutput> maybe_autorestore(HyprctlClient& client, const RuntimeConfig& runtime_config, const RuntimeStateProvider* state_provider,
                                                   const SessionCoordinator* session_coordinator) {
        if (!runtime_config.config.session_autorestore) {
            return std::nullopt;
        }
        return run_hyprctl_command(client, runtime_config, OutputFormat::kNormal, "session restore", nullptr, {}, nullptr, state_provider, session_coordinator, nullptr, nullptr,
                                   nullptr);
    }

} // namespace hyprspaces
