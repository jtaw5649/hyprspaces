#include "hyprspaces/state_provider.hpp"

#include <any>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#undef private

#include <hyprland/src/protocols/XDGShell.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>

#include "hyprspaces/strings.hpp"

namespace hyprspaces {

    namespace {

        std::optional<std::string> monitor_name_from_workspace(const PHLWORKSPACE& workspace) {
            if (!workspace || workspace->inert()) {
                return std::nullopt;
            }
            const auto monitor = workspace->m_monitor.lock();
            if (!monitor) {
                return std::nullopt;
            }
            return monitor->m_name;
        }

        std::string window_address(const PHLWINDOW& window) {
            std::ostringstream out;
            out << "0x" << std::hex << reinterpret_cast<uintptr_t>(window.get());
            return out.str();
        }

    } // namespace

    std::optional<int> current_active_workspace_id() {
        if (!g_pCompositor) {
            return std::nullopt;
        }
        const auto focus = Desktop::focusState();
        if (!focus) {
            return std::nullopt;
        }
        const auto monitor = focus->monitor();
        if (!monitor) {
            return std::nullopt;
        }
        return static_cast<int>(monitor->activeWorkspaceID());
    }

    std::optional<std::string> current_active_window_address() {
        if (!g_pCompositor) {
            return std::nullopt;
        }
        const auto focus = Desktop::focusState();
        if (!focus) {
            return std::nullopt;
        }
        const auto window = focus->window();
        if (!window || !window->m_isMapped) {
            return std::nullopt;
        }
        return window_address(window);
    }

    std::optional<std::vector<MonitorInfo>> current_monitors() {
        if (!g_pCompositor) {
            return std::nullopt;
        }
        std::vector<MonitorInfo> monitors;
        monitors.reserve(g_pCompositor->m_monitors.size());
        for (const auto& monitor : g_pCompositor->m_monitors) {
            if (!monitor) {
                continue;
            }
            std::optional<std::string> description;
            if (!monitor->m_description.empty()) {
                description = monitor->m_description;
            }
            std::optional<int> active_workspace_id;
            const auto         active_id = monitor->activeWorkspaceID();
            if (active_id > 0) {
                active_workspace_id = static_cast<int>(active_id);
            }
            monitors.push_back(MonitorInfo{
                .name                = monitor->m_name,
                .id                  = static_cast<int>(monitor->m_id),
                .x                   = static_cast<int>(monitor->m_position.x),
                .description         = description,
                .active_workspace_id = active_workspace_id,
            });
        }
        return monitors;
    }

    std::optional<std::vector<WorkspaceInfo>> current_workspaces() {
        if (!g_pCompositor) {
            return std::nullopt;
        }
        std::vector<WorkspaceInfo> workspaces;
        auto                       hypr_workspaces = g_pCompositor->getWorkspaces();
        for (const auto& workspace_ref : hypr_workspaces) {
            const auto workspace = workspace_ref.lock();
            if (!workspace || workspace->inert()) {
                continue;
            }
            std::optional<std::string> name;
            if (!workspace->m_name.empty()) {
                name = workspace->m_name;
            }
            workspaces.push_back(WorkspaceInfo{
                .id      = static_cast<int>(workspace->m_id),
                .windows = workspace->getWindows(),
                .name    = name,
                .monitor = monitor_name_from_workspace(workspace),
            });
        }
        return workspaces;
    }

    std::optional<std::vector<ClientInfo>> current_clients() {
        if (!g_pCompositor) {
            return std::nullopt;
        }
        std::vector<ClientInfo> clients;
        clients.reserve(g_pCompositor->m_windows.size());
        for (const auto& window : g_pCompositor->m_windows) {
            if (!window || !window->m_isMapped) {
                continue;
            }
            const auto                 workspace    = window->m_workspace;
            int                        workspace_id = -1;
            std::optional<std::string> workspace_name;
            if (workspace) {
                workspace_id = static_cast<int>(workspace->m_id);
                if (!workspace->m_name.empty()) {
                    workspace_name = workspace->m_name;
                }
            }

            std::optional<std::string> class_name;
            if (!window->m_class.empty()) {
                class_name = window->m_class;
            }
            std::optional<std::string> title;
            if (!window->m_title.empty()) {
                title = window->m_title;
            }
            std::optional<std::string> initial_class;
            if (!window->m_initialClass.empty()) {
                initial_class = window->m_initialClass;
            }
            std::optional<std::string> initial_title;
            if (!window->m_initialTitle.empty()) {
                initial_title = window->m_initialTitle;
            }
            std::optional<std::string> app_id;
            if (!window->m_isX11) {
                if (const auto xdg = window->m_xdgSurface.lock(); xdg && xdg->m_toplevel.lock()) {
                    const auto toplevel = xdg->m_toplevel.lock();
                    if (!toplevel->m_state.appid.empty()) {
                        app_id = toplevel->m_state.appid;
                    }
                }
            }
            std::optional<int> pid;
            const auto         window_pid = window->getPID();
            if (window_pid > 0) {
                pid = static_cast<int>(window_pid);
            }

            clients.push_back(ClientInfo{
                .address       = window_address(window),
                .workspace     = WorkspaceRef{.id = workspace_id, .name = workspace_name},
                .class_name    = class_name,
                .title         = title,
                .initial_class = initial_class,
                .initial_title = initial_title,
                .app_id        = app_id,
                .pid           = pid,
                .command       = std::nullopt,
                .geometry =
                    WindowGeometry{
                        .x      = static_cast<int>(window->m_realPosition->goal().x),
                        .y      = static_cast<int>(window->m_realPosition->goal().y),
                        .width  = static_cast<int>(window->m_realSize->goal().x),
                        .height = static_cast<int>(window->m_realSize->goal().y),
                    },
                .floating = window->m_isFloating,
                .pinned   = window->m_pinned,
                .fullscreen =
                    FullscreenState{
                        .internal = static_cast<int>(window->m_fullscreenState.internal),
                        .client   = static_cast<int>(window->m_fullscreenState.client),
                    },
                .urgent = window->m_isUrgent,
            });
        }
        return clients;
    }

    RuntimeStateProvider current_state_provider() {
        return RuntimeStateProvider{
            .active_workspace_id   = []() { return current_active_workspace_id(); },
            .active_window_address = []() { return current_active_window_address(); },
            .monitors              = []() { return current_monitors(); },
            .workspaces            = []() { return current_workspaces(); },
            .clients               = []() { return current_clients(); },
        };
    }

    WorkspaceBindings current_workspace_bindings() {
        if (!g_pConfigManager || !g_pCompositor) {
            return {};
        }
        std::vector<WorkspaceRuleBinding> rules;
        rules.reserve(g_pConfigManager->m_workspaceRules.size());
        for (const auto& rule : g_pConfigManager->m_workspaceRules) {
            if (rule.monitor.empty()) {
                continue;
            }
            std::optional<int> workspace_id;
            if (rule.workspaceId > 0) {
                workspace_id = static_cast<int>(rule.workspaceId);
            }
            std::optional<std::string> workspace_name;
            if (!rule.workspaceName.empty()) {
                workspace_name = rule.workspaceName;
            }
            if (!workspace_id && !workspace_name) {
                continue;
            }
            rules.push_back(WorkspaceRuleBinding{
                .workspace_id   = workspace_id,
                .workspace_name = workspace_name,
                .monitor        = rule.monitor,
            });
        }

        const auto workspaces = current_workspaces();
        return resolve_workspace_bindings(rules, workspaces ? *workspaces : std::vector<WorkspaceInfo>{});
    }

    FocusResolvers focus_resolvers() {
        FocusResolvers resolvers;
        resolvers.monitor_for_workspace = [](int workspace_id) -> std::optional<std::string> {
            if (!g_pCompositor) {
                return std::nullopt;
            }
            for (const auto& workspace : g_pCompositor->getWorkspaces()) {
                if (!workspace || workspace->inert()) {
                    continue;
                }
                if (workspace->m_id != workspace_id) {
                    continue;
                }
                const auto monitor = workspace->m_monitor.lock();
                if (!monitor) {
                    return std::nullopt;
                }
                return monitor->m_name;
            }
            return std::nullopt;
        };
        return resolvers;
    }

    WorkspaceResolver workspace_resolver() {
        return [](std::string_view spec, std::string* error) -> std::optional<int> {
            const auto trimmed = hyprspaces::trim_view(spec);
            if (trimmed.empty()) {
                if (error) {
                    *error = "missing workspace id";
                }
                return std::nullopt;
            }
            const auto result = getWorkspaceIDNameFromString(std::string(trimmed));
            if (result.id <= 0) {
                if (error) {
                    if (trimmed.starts_with("special")) {
                        *error = "special workspaces are not supported";
                    } else {
                        *error = "workspace target must be a numbered workspace";
                    }
                }
                return std::nullopt;
            }
            return static_cast<int>(result.id);
        };
    }

} // namespace hyprspaces
