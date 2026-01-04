#include <sstream>

#include <hyprland/src/plugins/PluginAPI.hpp>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#undef private

#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <source_location>
#include <vector>

#include <nlohmann/json.hpp>

#include "hyprspaces/config.hpp"
#include "hyprspaces/config_value.hpp"
#include "hyprspaces/dispatch_result.hpp"
#include "hyprspaces/events.hpp"
#include "hyprspaces/failsafe.hpp"
#include "hyprspaces/logging.hpp"
#include "hyprspaces/notifications.hpp"
#include "hyprspaces/paths.hpp"
#include "hyprspaces/plugin_entry.hpp"
#include "hyprspaces/persistent_workspaces.hpp"
#include "hyprspaces/registration.hpp"
#include "hyprspaces/runtime.hpp"
#include "hyprspaces/runtime_utils.hpp"
#include "hyprspaces/state_provider.hpp"
#include "hyprspaces/setup.hpp"
#include "hyprspaces/waybar.hpp"
#include "hyprspaces/strings.hpp"
#include "hyprspaces/version.hpp"
#include "hyprspaces/workspace_switch.hpp"
#include "plugin_state.hpp"

namespace {

    SHyprCtlCommand build_command(const std::string& name, bool exact, std::function<std::string(eHyprCtlOutputFormat, std::string)> handler) {
        return SHyprCtlCommand{
            .name  = name,
            .exact = exact,
            .fn    = std::move(handler),
        };
    }

    std::optional<hyprspaces::ConfigValueView> make_override_view(Hyprlang::CConfigValue* value) {
        if (!value || !value->m_bSetByUser) {
            return std::nullopt;
        }
        return hyprspaces::ConfigValueView{.set_by_user = true, .value = value->getValue()};
    }

    hyprspaces::ConfigValueView make_value_view(Hyprlang::CConfigValue* value) {
        if (!value) {
            return hyprspaces::ConfigValueView{};
        }
        return hyprspaces::ConfigValueView{.set_by_user = value->m_bSetByUser, .value = value->getValue()};
    }

    std::optional<std::string> read_config_string_override(Hyprlang::CConfigValue* value) {
        const auto view = make_override_view(value);
        if (!view) {
            return std::nullopt;
        }
        return hyprspaces::read_string_override(*view);
    }

    std::optional<int> read_config_int_override(Hyprlang::CConfigValue* value) {
        const auto view = make_override_view(value);
        if (!view) {
            return std::nullopt;
        }
        return hyprspaces::read_positive_int_override(*view);
    }

    std::optional<int> read_config_non_negative_int_override(Hyprlang::CConfigValue* value) {
        const auto view = make_override_view(value);
        if (!view) {
            return std::nullopt;
        }
        return hyprspaces::read_non_negative_int_override(*view);
    }

    std::optional<bool> read_config_bool_override(Hyprlang::CConfigValue* value) {
        const auto view = make_override_view(value);
        if (!view) {
            return std::nullopt;
        }
        return hyprspaces::read_bool_override(*view);
    }

    bool read_config_bool_value(Hyprlang::CConfigValue* value, bool default_value) {
        return hyprspaces::read_bool_value(make_value_view(value), default_value);
    }

    void debug_log_sink(std::string_view message) {
        if (Log::logger) {
            Log::logger->log(Log::DEBUG, std::string(message));
        }
    }

    void error_log_sink(std::string_view message) {
        if (Log::logger) {
            Log::logger->log(Log::ERR, std::string(message));
        }
    }

    std::optional<hyprspaces::SessionReason> read_session_reason_override(Hyprlang::CConfigValue* value) {
        const auto raw = read_config_string_override(value);
        if (!raw) {
            return std::nullopt;
        }
        return hyprspaces::parse_session_reason(*raw);
    }

    hyprspaces::ConfigOverrides current_overrides() {
        const auto selection = hyprspaces::select_monitor_groups(g_state.monitor_groups, g_state.monitor_profiles, hyprspaces::current_monitors());
        return hyprspaces::ConfigOverrides{
            .monitor_groups        = selection.groups,
            .workspace_count       = read_config_int_override(g_state.config.workspace_count),
            .wrap_cycling          = read_config_bool_override(g_state.config.wrap_cycling),
            .safe_mode             = read_config_bool_override(g_state.config.safe_mode),
            .debug_logging         = read_config_bool_override(g_state.config.debug_logging),
            .waybar_enabled        = read_config_bool_override(g_state.config.waybar_enabled),
            .waybar_theme_css      = read_config_string_override(g_state.config.waybar_theme_css),
            .waybar_class          = read_config_string_override(g_state.config.waybar_class),
            .session_enabled       = read_config_bool_override(g_state.config.session_enabled),
            .session_autosave      = read_config_bool_override(g_state.config.session_autosave),
            .session_autorestore   = read_config_bool_override(g_state.config.session_autorestore),
            .session_reason        = read_session_reason_override(g_state.config.session_reason),
            .persistent_all        = read_config_bool_override(g_state.config.persistent_all),
            .persistent_workspaces = g_state.persistent_workspaces,
        };
    }

    struct NotificationSettings {
        bool session    = true;
        int  timeout_ms = 3000;
    };

    NotificationSettings current_notification_settings() {
        NotificationSettings settings;
        if (const auto override = read_config_bool_override(g_state.config.notify_session)) {
            settings.session = *override;
        }
        if (const auto override = read_config_non_negative_int_override(g_state.config.notify_timeout_ms)) {
            settings.timeout_ms = *override;
        }
        return settings;
    }

    void                      mark_waybar_update();
    void                      emit_waybar_update();

    hyprspaces::RuntimeConfig load_cached_runtime_config() {
        if (!g_state.runtime_cache) {
            g_state.runtime_cache.emplace(g_state.paths);
        }
        const auto& runtime_config = g_state.runtime_cache->get(current_overrides(), hyprspaces::current_workspace_bindings());
        return runtime_config;
    }

    hyprspaces::OutputFormat to_output_format(eHyprCtlOutputFormat format) {
        return format == eHyprCtlOutputFormat::FORMAT_JSON ? hyprspaces::OutputFormat::kJson : hyprspaces::OutputFormat::kNormal;
    }

    std::string_view strip_command_prefix(std::string_view request, std::string_view prefix) {
        if (request.starts_with(prefix)) {
            request.remove_prefix(prefix.size());
        }
        return hyprspaces::trim_view(request);
    }

    std::optional<hyprspaces::Command> parse_prefixed_command(std::string_view prefix, std::string_view args, std::string* error) {
        const auto command = hyprspaces::build_prefixed_command(prefix, args);
        const auto parsed  = hyprspaces::parse_command(command);
        if (std::holds_alternative<hyprspaces::ParseError>(parsed)) {
            if (error) {
                *error = std::get<hyprspaces::ParseError>(parsed).message;
            }
            return std::nullopt;
        }
        return std::get<hyprspaces::Command>(parsed);
    }

    std::string render_error(eHyprCtlOutputFormat format, std::string_view message) {
        if (format != eHyprCtlOutputFormat::FORMAT_JSON) {
            return std::string(message);
        }
        nlohmann::json json{{"status", "error"}, {"message", message}};
        return json.dump();
    }

    void notify_error(std::string_view context, std::string_view message, const std::source_location& location = std::source_location::current()) {
        hyprspaces::error_log(context, message, location);
        const std::string text = hyprspaces::format_log_entry(context, message);
        HyprlandAPI::addNotification(PHANDLE, text, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
    }

    Hyprlang::CParseResult handle_monitor_group_keyword(const char*, const char* value) {
        Hyprlang::CParseResult result;
        try {
            const auto group = hyprspaces::parse_monitor_group(value ? value : "");
            for (const auto& existing : g_state.monitor_groups) {
                const auto existing_profile = existing.profile.value_or(std::string{});
                const auto incoming_profile = group.profile.value_or(std::string{});
                if (existing_profile != incoming_profile) {
                    continue;
                }
                if (existing.name == group.name) {
                    result.setError("monitor group name already defined");
                    return result;
                }
                for (const auto& monitor : group.monitors) {
                    if (std::find(existing.monitors.begin(), existing.monitors.end(), monitor) != existing.monitors.end()) {
                        result.setError("monitor already assigned to another group");
                        return result;
                    }
                }
            }
            g_state.monitor_groups.push_back(group);
        } catch (const std::exception& ex) { result.setError(ex.what()); }
        return result;
    }

    Hyprlang::CParseResult handle_monitor_profile_keyword(const char*, const char* value) {
        Hyprlang::CParseResult result;
        try {
            const auto profile = hyprspaces::parse_monitor_profile(value ? value : "");
            for (const auto& existing : g_state.monitor_profiles) {
                if (existing.name == profile.name) {
                    result.setError("monitor profile name already defined");
                    return result;
                }
            }
            g_state.monitor_profiles.push_back(profile);
        } catch (const std::exception& ex) { result.setError(ex.what()); }
        return result;
    }

    Hyprlang::CParseResult handle_persistent_workspace_keyword(const char*, const char* value) {
        Hyprlang::CParseResult result;
        try {
            const auto parsed = hyprspaces::parse_persistent_workspace(value ? value : "");
            if (!parsed) {
                result.setError("invalid persistent workspace");
                return result;
            }
            if (std::find(g_state.persistent_workspaces.begin(), g_state.persistent_workspaces.end(), *parsed) != g_state.persistent_workspaces.end()) {
                result.setError("persistent workspace already defined");
                return result;
            }
            g_state.persistent_workspaces.push_back(*parsed);
        } catch (const std::exception& ex) { result.setError(ex.what()); }
        return result;
    }

    Hyprlang::CParseResult handle_profile_keyword(const char*, const char* value) {
        Hyprlang::CParseResult result;
        try {
            const auto name = hyprspaces::parse_profile_name(value ? value : "");
            g_state.profile_catalog.set_active_profile(name);
        } catch (const std::exception& ex) { result.setError(ex.what()); }
        return result;
    }

    Hyprlang::CParseResult handle_profile_description_keyword(const char*, const char* value) {
        Hyprlang::CParseResult result;
        try {
            const auto description = hyprspaces::parse_profile_description(value ? value : "");
            const auto active      = g_state.profile_catalog.active_profile();
            if (!active) {
                result.setError("profile name is required");
                return result;
            }
            g_state.profile_catalog.set_description(*active, description);
        } catch (const std::exception& ex) { result.setError(ex.what()); }
        return result;
    }

    Hyprlang::CParseResult handle_profile_workspace_keyword(const char*, const char* value) {
        Hyprlang::CParseResult result;
        try {
            const auto active = g_state.profile_catalog.active_profile();
            const auto spec   = hyprspaces::parse_profile_workspace(value ? value : "", active ? std::optional<std::string>(*active) : std::nullopt);
            g_state.profile_catalog.add_workspace(spec);
        } catch (const std::exception& ex) { result.setError(ex.what()); }
        return result;
    }

    Hyprlang::CParseResult handle_profile_window_keyword(const char*, const char* value) {
        Hyprlang::CParseResult result;
        try {
            const auto active = g_state.profile_catalog.active_profile();
            const auto spec   = hyprspaces::parse_profile_window(value ? value : "", active ? std::optional<std::string>(*active) : std::nullopt);
            g_state.profile_catalog.add_window(spec);
        } catch (const std::exception& ex) { result.setError(ex.what()); }
        return result;
    }

    std::optional<std::string> register_config_keywords() {
        Hyprlang::SHandlerOptions options;
        const bool                ok = HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:hyprspaces:monitor_group", handle_monitor_group_keyword, options);
        if (!ok) {
            return "failed to register config keyword";
        }
        const bool ok_profile = HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:hyprspaces:monitor_profile", handle_monitor_profile_keyword, options);
        if (!ok_profile) {
            return "failed to register config keyword";
        }
        const bool ok_persistent = HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:hyprspaces:persistent_workspace", handle_persistent_workspace_keyword, options);
        if (!ok_persistent) {
            return "failed to register config keyword";
        }
        const bool ok_profile_name = HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:hyprspaces:profile", handle_profile_keyword, options);
        if (!ok_profile_name) {
            return "failed to register config keyword";
        }
        const bool ok_profile_description = HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:hyprspaces:profile_description", handle_profile_description_keyword, options);
        if (!ok_profile_description) {
            return "failed to register config keyword";
        }
        const bool ok_profile_workspace = HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:hyprspaces:profile_workspace", handle_profile_workspace_keyword, options);
        if (!ok_profile_workspace) {
            return "failed to register config keyword";
        }
        const bool ok_profile_window = HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:hyprspaces:profile_window", handle_profile_window_keyword, options);
        if (!ok_profile_window) {
            return "failed to register config keyword";
        }
        return std::nullopt;
    }

    void disable_plugin(std::string_view reason) {
        g_state.enabled = false;
        hyprspaces::error_log("disabled", reason);
        if (g_state.waybar_bridge) {
            g_state.waybar_bridge->stop();
        }
        g_state.hooks.clear();
    }

    bool plugin_enabled() {
        return g_state.enabled;
    }

    bool notify_session_command(const hyprspaces::Command& command, const hyprspaces::CommandOutput& output) {
        if (command.kind != hyprspaces::CommandKind::kSessionSave && command.kind != hyprspaces::CommandKind::kSessionRestore) {
            return false;
        }

        const auto settings = current_notification_settings();
        if (!settings.session) {
            return false;
        }

        const auto action = command.kind == hyprspaces::CommandKind::kSessionSave ? hyprspaces::SessionAction::kSave : hyprspaces::SessionAction::kRestore;
        const auto detail = output.success ? std::string_view{} : std::string_view(output.output);
        const auto text   = hyprspaces::session_notification_text(action, output.success, "", detail);
        if (Log::logger) {
            Log::logger->log(output.success ? Log::DEBUG : Log::ERR, text);
        }
        const auto color = output.success ? CHyprColor{0.2, 0.8, 0.2, 1.0} : CHyprColor{1.0, 0.2, 0.2, 1.0};
        HyprlandAPI::addNotification(PHANDLE, text, color, settings.timeout_ms);
        return true;
    }

    std::optional<std::string> switch_monitor_workspace(PHLMONITOR monitor, int workspace_id, bool focus) {
        if (!monitor) {
            return std::string("missing monitor");
        }
        auto workspace = g_pCompositor->getWorkspaceByID(workspace_id);
        if (!workspace) {
            workspace = g_pCompositor->createNewWorkspace(workspace_id, monitor->m_id, "", false);
            if (!workspace) {
                return std::string("failed to create workspace");
            }
        }
        if (monitor->m_activeWorkspace == workspace) {
            return std::nullopt;
        }
        monitor->changeWorkspace(workspace, false, true, !focus);
        return std::nullopt;
    }

    std::optional<std::string> apply_workspace_switch_plan(const hyprspaces::WorkspaceSwitchPlan& plan) {
        if (!g_pCompositor) {
            return std::string("missing compositor");
        }
        if (plan.targets.empty()) {
            return std::string("missing workspace targets");
        }

        PHLMONITOR focus_monitor = nullptr;
        if (!plan.focus_monitor.empty()) {
            focus_monitor = g_pCompositor->getMonitorFromString(plan.focus_monitor);
        }
        if (!focus_monitor) {
            focus_monitor = g_pCompositor->getMonitorFromString(plan.targets.back().monitor);
        }
        if (!focus_monitor) {
            return std::string("missing monitor for workspace switch");
        }

        Desktop::focusState()->rawMonitorFocus(focus_monitor);

        auto switch_target = [&](const hyprspaces::MonitorWorkspaceTarget& target) -> std::optional<std::string> {
            const auto monitor = g_pCompositor->getMonitorFromString(target.monitor);
            if (!monitor) {
                return std::string("missing monitor for workspace switch");
            }
            const bool focus = monitor == focus_monitor;
            return switch_monitor_workspace(monitor, target.workspace_id, focus);
        };

        for (const auto& target : plan.targets) {
            if (focus_monitor && target.monitor == focus_monitor->m_name) {
                continue;
            }
            if (const auto error = switch_target(target)) {
                return error;
            }
        }

        for (const auto& target : plan.targets) {
            if (focus_monitor && target.monitor == focus_monitor->m_name) {
                return switch_target(target);
            }
        }
        return std::nullopt;
    }

    const hyprspaces::WorkspaceSwitchExecutor& workspace_switch_executor() {
        static const hyprspaces::WorkspaceSwitchExecutor executor = [](const hyprspaces::WorkspaceSwitchPlan& plan) -> std::optional<std::string> {
            return apply_workspace_switch_plan(plan);
        };
        return executor;
    }

    const hyprspaces::SessionCoordinator* session_coordinator() {
        if (!g_state.session_coordinator) {
            return nullptr;
        }
        return &(*g_state.session_coordinator);
    }

    SDispatchResult to_dispatch_result(const hyprspaces::CommandOutput& output) {
        const auto result = hyprspaces::dispatch_result_from_output(output);
        return SDispatchResult{
            .passEvent = result.pass_event,
            .success   = result.success,
            .error     = result.error,
        };
    }

    SDispatchResult run_dispatcher_result(std::string_view args, std::string_view prefix, std::string_view context) {
        if (!plugin_enabled()) {
            return to_dispatch_result(hyprspaces::CommandOutput{false, "hyprspaces disabled"});
        }
        SDispatchResult result = to_dispatch_result(hyprspaces::CommandOutput{false, "hyprspaces disabled"});
        (void)hyprspaces::failsafe::guard(
            [&]() {
                auto                      runtime_config = load_cached_runtime_config();
                hyprspaces::HyprctlClient client(g_invoker);
                const auto                resolver    = hyprspaces::workspace_resolver();
                const auto&               executor    = workspace_switch_executor();
                const auto*               coordinator = session_coordinator();
                const auto output = hyprspaces::run_prefixed_command(client, runtime_config, prefix, args, resolver, &g_state.workspace_history, nullptr, coordinator,
                                                                     &g_state.profile_catalog, &executor, &g_state.marks);
                if (!output.success) {
                    notify_error(context, output.output);
                }
                result = to_dispatch_result(output);
            },
            fail_fast, context);
        return result;
    }

    SDispatchResult run_session_dispatcher(std::string_view args, std::string_view prefix, std::string_view context, hyprspaces::SessionAction action) {
        if (!plugin_enabled()) {
            return to_dispatch_result(hyprspaces::CommandOutput{false, "hyprspaces disabled"});
        }
        SDispatchResult result = to_dispatch_result(hyprspaces::CommandOutput{false, "hyprspaces disabled"});
        (void)hyprspaces::failsafe::guard(
            [&]() {
                auto                      runtime_config = load_cached_runtime_config();
                hyprspaces::HyprctlClient client(g_invoker);
                const auto                resolver    = hyprspaces::workspace_resolver();
                const auto&               executor    = workspace_switch_executor();
                const auto*               coordinator = session_coordinator();
                const auto output = hyprspaces::run_prefixed_command(client, runtime_config, prefix, args, resolver, &g_state.workspace_history, nullptr, coordinator,
                                                                     &g_state.profile_catalog, &executor, &g_state.marks);

                const auto parsed   = parse_prefixed_command(prefix, args, nullptr);
                bool       notified = false;
                if (parsed) {
                    notified = notify_session_command(*parsed, output);
                } else if (!output.success) {
                    const auto settings = current_notification_settings();
                    if (settings.session) {
                        const auto text = hyprspaces::session_notification_text(action, false, "", output.output);
                        HyprlandAPI::addNotification(PHANDLE, text, CHyprColor{1.0, 0.2, 0.2, 1.0}, settings.timeout_ms);
                        notified = true;
                    }
                }

                if (!output.success && !notified) {
                    notify_error(context, output.output);
                }
                result = to_dispatch_result(output);
            },
            fail_fast, context);
        return result;
    }

    hyprspaces::SetupActions make_setup_actions() {
        return hyprspaces::SetupActions{
            .apply_workspace_rules = [](const std::vector<std::string>& lines) -> std::optional<std::string> {
                if (!g_pConfigManager) {
                    return "config manager unavailable";
                }
                std::erase_if(g_pConfigManager->m_workspaceRules, [](const SWorkspaceRule& rule) { return rule.layoutopts.contains("hyprspaces"); });
                for (const auto& line : lines) {
                    if (const auto error = g_pConfigManager->handleWorkspaceRules("workspace", line)) {
                        return error;
                    }
                }
                g_pConfigManager->ensurePersistentWorkspacesPresent();
                return std::nullopt;
            },
            .clear_workspace_rules = []() -> std::optional<std::string> {
                if (!g_pConfigManager) {
                    return "config manager unavailable";
                }
                std::erase_if(g_pConfigManager->m_workspaceRules, [](const SWorkspaceRule& rule) { return rule.layoutopts.contains("hyprspaces"); });
                return std::nullopt;
            },
            .install_waybar_assets = [](const hyprspaces::Paths& paths, const std::optional<std::string>& theme_override) -> std::optional<std::string> {
                return hyprspaces::install_waybar_assets(paths, theme_override);
            },
            .uninstall_waybar_assets = [](const hyprspaces::Paths& paths) -> std::optional<std::string> { return hyprspaces::uninstall_waybar_assets(paths); },
        };
    }

    std::optional<hyprspaces::FocusEvent> make_focus_event(std::chrono::steady_clock::time_point now, std::optional<int> workspace_id, std::optional<std::string> monitor_name) {
        if (!workspace_id || *workspace_id <= 0) {
            return std::nullopt;
        }
        return hyprspaces::FocusEvent{
            .at             = now,
            .workspace_id   = workspace_id,
            .window_address = std::nullopt,
            .monitor_name   = std::move(monitor_name),
        };
    }

    std::optional<hyprspaces::FocusEvent> focus_event_from_workspace(std::chrono::steady_clock::time_point now, const PHLWORKSPACE& workspace) {
        if (!workspace || workspace->inert()) {
            return std::nullopt;
        }
        const auto monitor = workspace->m_monitor.lock();
        if (!monitor) {
            return std::nullopt;
        }
        return make_focus_event(now, static_cast<int>(workspace->m_id), monitor->m_name);
    }

    std::optional<hyprspaces::FocusEvent> focus_event_from_monitor(std::chrono::steady_clock::time_point now, const PHLMONITOR& monitor) {
        if (!monitor || !monitor->m_activeWorkspace) {
            return std::nullopt;
        }
        return make_focus_event(now, static_cast<int>(monitor->m_activeWorkspace->m_id), monitor->m_name);
    }

    std::optional<hyprspaces::FocusEvent> focus_event_from_state(std::chrono::steady_clock::time_point now) {
        if (!g_pCompositor) {
            return std::nullopt;
        }
        const auto focus = Desktop::focusState();
        if (!focus) {
            return std::nullopt;
        }
        const auto monitor = focus->monitor();
        if (!monitor || !monitor->m_activeWorkspace) {
            return std::nullopt;
        }
        return make_focus_event(now, static_cast<int>(monitor->m_activeWorkspace->m_id), monitor->m_name);
    }

    void handle_focus_event(const std::optional<hyprspaces::FocusEvent>& event) {
        auto                                  runtime_config = load_cached_runtime_config();
        hyprspaces::HyprctlClient             client(g_invoker);
        std::optional<hyprspaces::FocusEvent> resolved = event;
        if (!resolved) {
            const auto now = std::chrono::steady_clock::now();
            resolved       = focus_event_from_state(now);
            if (!resolved) {
                const auto workspace_id = client.active_workspace_id();
                if (!workspace_id) {
                    hyprspaces::error_log(workspace_id.error().context, workspace_id.error().message);
                    return;
                }
                resolved = hyprspaces::FocusEvent{
                    .at             = now,
                    .workspace_id   = *workspace_id,
                    .window_address = std::nullopt,
                    .monitor_name   = std::nullopt,
                };
            }
        }
        const auto  resolvers = hyprspaces::focus_resolvers();
        const auto& executor  = workspace_switch_executor();
        hyprspaces::focus_switch_for_event(client, runtime_config.config, *resolved, g_state.focus_debounce, resolvers, runtime_config.bindings, &g_state.workspace_history,
                                           &executor);
        mark_waybar_update();
    }

    void handle_monitor_event(hyprspaces::MonitorEventKind kind) {
        auto                      runtime_config = load_cached_runtime_config();
        hyprspaces::HyprctlClient client(g_invoker);
        hyprspaces::rebalance_for_event(client, runtime_config.config, kind, g_state.rebalance_debounce, std::chrono::steady_clock::now(), runtime_config.bindings);
        mark_waybar_update();
    }

    void handle_tick_event() {
        const auto now = std::chrono::steady_clock::now();
        if (g_state.rebalance_debounce.flush(now)) {
            auto                      runtime_config = load_cached_runtime_config();
            hyprspaces::HyprctlClient client(g_invoker);
            const auto                commands = hyprspaces::rebalance_sequence(runtime_config.config, runtime_config.bindings);
            const auto                output   = client.dispatch_batch(commands);
            if (!hyprspaces::is_ok_response(output)) {
                notify_error("rebalance", output);
            }
        }
        if (g_state.waybar_debounce.flush(now)) {
            emit_waybar_update();
        }
    }

    void mark_waybar_update() {
        g_state.waybar_debounce.mark(std::chrono::steady_clock::now());
    }

    void emit_waybar_update() {
        if (!g_state.waybar_bridge || !g_state.waybar_bridge->running()) {
            return;
        }
        auto        runtime_config = load_cached_runtime_config();
        const auto  theme_path     = resolve_waybar_theme_path(runtime_config, std::nullopt);
        std::string theme_error;
        const auto  theme = g_state.waybar_theme_cache.load(theme_path, &theme_error);
        if (!theme) {
            notify_error("waybar", theme_error.empty() ? "invalid waybar theme css" : theme_error);
            return;
        }

        auto active_workspace = hyprspaces::current_active_workspace_id();
        auto workspaces       = hyprspaces::current_workspaces();
        auto monitors         = hyprspaces::current_monitors();
        auto clients          = hyprspaces::current_clients();
        if (!active_workspace || !workspaces || !monitors || !clients) {
            hyprspaces::HyprctlClient client(g_invoker);
            if (!active_workspace) {
                const auto workspace_id = client.active_workspace_id();
                if (!workspace_id) {
                    hyprspaces::error_log(workspace_id.error().context, workspace_id.error().message);
                    return;
                }
                active_workspace = *workspace_id;
            }
            if (!workspaces) {
                const auto resolved = client.workspaces();
                if (!resolved) {
                    hyprspaces::error_log(resolved.error().context, resolved.error().message);
                    return;
                }
                workspaces = *resolved;
            }
            if (!monitors) {
                const auto resolved = client.monitors();
                if (!resolved) {
                    hyprspaces::error_log(resolved.error().context, resolved.error().message);
                    return;
                }
                monitors = *resolved;
            }
            if (!clients) {
                const auto resolved = client.clients();
                if (!resolved) {
                    hyprspaces::error_log(resolved.error().context, resolved.error().message);
                    return;
                }
                clients = *resolved;
            }
        }
        const auto output = hyprspaces::render_waybar_json(runtime_config.config, *active_workspace, *workspaces, *monitors, *clients, *theme);
        g_state.waybar_bridge->broadcast(output);
    }

    void register_event_hooks() {
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "preConfigReload", [](void*, SCallbackInfo&, std::any) {
            g_state.monitor_groups.clear();
            g_state.monitor_profiles.clear();
            g_state.persistent_workspaces.clear();
            g_state.profile_catalog.clear();
            if (g_state.runtime_cache) {
                g_state.runtime_cache->invalidate();
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorAdded", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { handle_monitor_event(hyprspaces::MonitorEventKind::kAdded); }, fail_fast, "monitor added");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorRemoved", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { handle_monitor_event(hyprspaces::MonitorEventKind::kRemoved); }, fail_fast, "monitor removed");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "workspace", [](void*, SCallbackInfo&, std::any data) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard(
                [&]() {
                    const auto now       = std::chrono::steady_clock::now();
                    const auto workspace = std::any_cast<PHLWORKSPACE>(&data);
                    handle_focus_event(workspace ? focus_event_from_workspace(now, *workspace) : std::nullopt);
                },
                fail_fast, "workspace focus");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "focusedMon", [](void*, SCallbackInfo&, std::any data) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard(
                [&]() {
                    const auto now     = std::chrono::steady_clock::now();
                    const auto monitor = std::any_cast<PHLMONITOR>(&data);
                    handle_focus_event(monitor ? focus_event_from_monitor(now, *monitor) : std::nullopt);
                },
                fail_fast, "monitor focus");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "createWorkspace", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { mark_waybar_update(); }, fail_fast, "workspace create");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "destroyWorkspace", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { mark_waybar_update(); }, fail_fast, "workspace destroy");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { mark_waybar_update(); }, fail_fast, "window open");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { mark_waybar_update(); }, fail_fast, "window close");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "moveWindow", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { mark_waybar_update(); }, fail_fast, "window move");
            if (!ok) {
                return;
            }
        }));
        g_state.hooks.push_back(HyprlandAPI::registerCallbackDynamic(PHANDLE, "tick", [](void*, SCallbackInfo&, std::any) {
            if (!plugin_enabled()) {
                return;
            }
            const bool ok = hyprspaces::failsafe::guard([&]() { handle_tick_event(); }, fail_fast, "tick");
            if (!ok) {
                return;
            }
        }));
    }

    std::optional<std::string> register_config_values() {
        for (const auto& spec : hyprspaces::config_specs()) {
            bool ok = false;
            switch (spec.type) {
                case hyprspaces::ConfigValueType::kString: ok = HyprlandAPI::addConfigValue(PHANDLE, spec.name, Hyprlang::STRING{""}); break;
                case hyprspaces::ConfigValueType::kInt: ok = HyprlandAPI::addConfigValue(PHANDLE, spec.name, Hyprlang::INT{std::get<int>(spec.default_value)}); break;
                case hyprspaces::ConfigValueType::kBool: ok = HyprlandAPI::addConfigValue(PHANDLE, spec.name, Hyprlang::INT{std::get<bool>(spec.default_value) ? 1 : 0}); break;
            }
            if (!ok) {
                return "failed to register config value";
            }
        }
        return std::nullopt;
    }

    void cache_config_values() {
        g_state.config.workspace_count     = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:workspace_count");
        g_state.config.wrap_cycling        = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:wrap_cycling");
        g_state.config.safe_mode           = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:safe_mode");
        g_state.config.debug_logging       = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:debug_logging");
        g_state.config.waybar_enabled      = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:waybar_enabled");
        g_state.config.waybar_theme_css    = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:waybar_theme_css");
        g_state.config.waybar_class        = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:waybar_class");
        g_state.config.notify_session      = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:notify_session");
        g_state.config.notify_timeout_ms   = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:notify_timeout_ms");
        g_state.config.session_enabled     = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:session_enabled");
        g_state.config.session_autosave    = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:session_autosave");
        g_state.config.session_autorestore = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:session_autorestore");
        g_state.config.session_reason      = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:session_reason");
        g_state.config.persistent_all      = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprspaces:persistent_all");
    }

    void maybe_start_session_protocol() {
        if (!read_config_bool_value(g_state.config.session_enabled, false)) {
            return;
        }
        if (g_state.session_protocol) {
            return;
        }
        g_state.session_protocol = std::make_unique<hyprspaces::SessionProtocolServer>(g_state.paths.session_store_path);
        g_state.session_protocol->install_layout_hooks(PHANDLE);
        auto* server                = g_state.session_protocol.get();
        g_state.session_coordinator = hyprspaces::SessionCoordinator{
            .save    = [server](std::string_view id) { return server->save(id); },
            .restore = [server](std::string_view id) { return server->restore(id); },
        };
    }

    void register_dispatchers() {
        HyprlandAPI::addDispatcherV2(PHANDLE, "hyprspaces:paired_switch", [](std::string arg) { return run_dispatcher_result(arg, "paired switch", "paired switch"); });
        HyprlandAPI::addDispatcherV2(PHANDLE, "hyprspaces:paired_cycle", [](std::string arg) { return run_dispatcher_result(arg, "paired cycle", "paired cycle"); });
        HyprlandAPI::addDispatcherV2(PHANDLE, "hyprspaces:paired_move_window",
                                     [](std::string arg) { return run_dispatcher_result(arg, "paired move-window", "paired move-window"); });
        HyprlandAPI::addDispatcherV2(PHANDLE, "hyprspaces:session_save",
                                     [](std::string arg) { return run_session_dispatcher(arg, "session save", "session save", hyprspaces::SessionAction::kSave); });
        HyprlandAPI::addDispatcherV2(PHANDLE, "hyprspaces:session_restore",
                                     [](std::string arg) { return run_session_dispatcher(arg, "session restore", "session restore", hyprspaces::SessionAction::kRestore); });
    }

    void register_hyprctl_command() {
        HyprlandAPI::registerHyprCtlCommand(PHANDLE, build_command("hyprspaces", false, [](eHyprCtlOutputFormat format, std::string request) -> std::string {
                                                std::string response;
                                                const bool  ok = hyprspaces::failsafe::guard(
                                                    [&]() {
                                                        if (!plugin_enabled()) {
                                                            response = render_error(format, "hyprspaces disabled");
                                                            return;
                                                        }
                                                        const auto args   = strip_command_prefix(request, "hyprspaces");
                                                        const auto parsed = hyprspaces::parse_command(args);
                                                        if (std::holds_alternative<hyprspaces::ParseError>(parsed)) {
                                                            response = render_error(format, std::get<hyprspaces::ParseError>(parsed).message);
                                                            return;
                                                        }

                                                        hyprspaces::HyprctlClient client(g_invoker);
                                                        const auto                command = std::get<hyprspaces::Command>(parsed);
                                                        if (command.kind == hyprspaces::CommandKind::kSetupInstall) {
                                                            hyprspaces::ensure_hyprspaces_bootstrap(g_state.paths);
                                                        }

                                                        const auto  setup_actions = make_setup_actions();

                                                        auto        runtime_config = load_cached_runtime_config();
                                                        const auto  provider       = hyprspaces::current_state_provider();
                                                        const auto& executor       = workspace_switch_executor();
                                                        const auto* coordinator    = session_coordinator();
                                                        const auto  output = hyprspaces::run_hyprctl_command(client, runtime_config, to_output_format(format), args, &setup_actions,
                                                                                                              hyprspaces::workspace_resolver(), &g_state.workspace_history, &provider,
                                                                                                              coordinator, &g_state.profile_catalog, &executor, &g_state.marks);
                                                        notify_session_command(command, output);
                                                        if (!output.success) {
                                                            response = render_error(format, output.output);
                                                            return;
                                                        }
                                                        response = output.output;
                                                    },
                                                    fail_fast, "hyprctl");
                                                if (!ok) {
                                                    return render_error(format, "hyprspaces disabled");
                                                }
                                                return response;
                                            }));
    }

} // namespace

void fail_fast(std::string_view context, std::string_view message) {
    const std::string_view reason = message.empty() ? std::string_view{"unknown error"} : message;
    notify_error(context, reason);
    disable_plugin(reason);
}

namespace hyprspaces::plugin {

    PluginInitResult init(HANDLE handle) {
        PHANDLE         = handle;
        g_state.enabled = false;

        const std::string hash        = __hyprland_api_get_hash();
        const std::string client_hash = __hyprland_api_get_client_hash();

        if (hash != client_hash) {
            return {"hyprspaces", "Header mismatch - plugin disabled", "jtaw", std::string(hyprspaces::kPluginVersion)};
        }

        hyprspaces::set_debug_log_sink(debug_log_sink);
        hyprspaces::set_error_log_sink(error_log_sink);

        const auto paths = hyprspaces::try_resolve_paths_from_env();
        if (!paths) {
            notify_error("init", "missing HOME/XDG_CONFIG_HOME");
            return {"hyprspaces", "Init failed", "jtaw", std::string(hyprspaces::kPluginVersion)};
        }

        const bool init_ok = hyprspaces::failsafe::guard(
            [&]() {
                g_state.paths = *paths;
                g_state.runtime_cache.emplace(g_state.paths);
                hyprspaces::ensure_hyprspaces_bootstrap(g_state.paths);
                if (const auto error = register_config_values()) {
                    throw std::runtime_error(*error);
                }
                if (const auto error = register_config_keywords()) {
                    throw std::runtime_error(*error);
                }
                cache_config_values();
                maybe_start_session_protocol();
                register_dispatchers();
                register_hyprctl_command();
                register_event_hooks();
                {
                    auto                      runtime_config = load_cached_runtime_config();
                    hyprspaces::HyprctlClient client(g_invoker);
                    const auto                setup_actions = make_setup_actions();
                    if (const auto error = hyprspaces::ensure_workspace_rules(client, runtime_config, &setup_actions)) {
                        notify_error("setup install", *error);
                    }
                    if (const auto error = hyprspaces::ensure_cursor_no_warps(client)) {
                        notify_error("cursor", *error);
                    }
                }
                if (read_config_bool_value(g_state.config.waybar_enabled, true)) {
                    g_state.waybar_bridge.emplace(g_state.paths.waybar_dir / "waybar.sock");
                    if (const auto error = g_state.waybar_bridge->start()) {
                        throw std::runtime_error(*error);
                    }
                    emit_waybar_update();
                }
            },
            fail_fast, "plugin init");
        if (!init_ok) {
            return {"hyprspaces", "Init failed", "jtaw", std::string(hyprspaces::kPluginVersion)};
        }
        g_state.enabled = true;

        const bool restore_ok = hyprspaces::failsafe::guard(
            [&]() {
                auto                      runtime_config = load_cached_runtime_config();
                const auto                provider       = hyprspaces::current_state_provider();
                hyprspaces::HyprctlClient client(g_invoker);
                const auto                output = hyprspaces::maybe_autorestore(client, runtime_config, &provider, session_coordinator());
                if (output && !output->success) {
                    throw std::runtime_error(output->output);
                }
            },
            fail_fast, "auto restore");
        if (!restore_ok) {
            return {"hyprspaces", "Init failed", "jtaw", std::string(hyprspaces::kPluginVersion)};
        }

        return {"hyprspaces", "Paired workspaces and session utilities", "jtaw", std::string(hyprspaces::kPluginVersion)};
    }

    void exit() {
        const bool exit_ok = hyprspaces::failsafe::guard(
            [&]() {
                if (!g_state.enabled) {
                    return;
                }
                auto                      runtime_config = load_cached_runtime_config();
                const auto                provider       = hyprspaces::current_state_provider();
                hyprspaces::HyprctlClient client(g_invoker);
                const auto                output = hyprspaces::maybe_autosave(client, runtime_config, &provider, session_coordinator());
                if (output && !output->success) {
                    throw std::runtime_error(output->output);
                }
                if (g_state.waybar_bridge) {
                    g_state.waybar_bridge->stop();
                }
                if (g_pConfigManager) {
                    std::erase_if(g_pConfigManager->m_workspaceRules, [](const SWorkspaceRule& rule) { return rule.layoutopts.contains("hyprspaces"); });
                    g_pConfigManager->ensurePersistentWorkspacesPresent();
                }
                g_state.hooks.clear();
                g_state.session_coordinator.reset();
                g_state.session_protocol.reset();
            },
            fail_fast, "plugin exit");
        if (!exit_ok) {
            hyprspaces::clear_debug_log_sink();
            hyprspaces::clear_error_log_sink();
            return;
        }
        hyprspaces::clear_debug_log_sink();
        hyprspaces::clear_error_log_sink();
    }

} // namespace hyprspaces::plugin
