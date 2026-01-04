#include "session_protocol_server.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#define private public
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/protocols/XDGShell.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#undef private

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>

#include "xx-session-management-v1-protocol.h"

#include "hyprspaces/layout_capture.hpp"
#include "layout_restore.hpp"

namespace hyprspaces {

    namespace {

        void                   session_manager_destroy(wl_client* client, wl_resource* resource);
        void                   session_manager_get_session(wl_client* client, wl_resource* resource, uint32_t id, uint32_t reason, const char* session);
        void                   session_destroy(wl_client* client, wl_resource* resource);
        void                   session_remove(wl_client* client, wl_resource* resource);
        void                   session_add_toplevel(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* toplevel, const char* name);
        void                   session_restore_toplevel(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* toplevel, const char* name);
        void                   toplevel_destroy(wl_client* client, wl_resource* resource);
        void                   toplevel_remove(wl_client* client, wl_resource* resource);
        void                   destroy_session_resource(wl_resource* resource);
        void                   destroy_toplevel_resource(wl_resource* resource);

        SessionProtocolServer* g_active_server     = nullptr;
        CFunctionHook*         g_dwindle_hook      = nullptr;
        CFunctionHook*         g_master_hook       = nullptr;
        CFunctionHook*         g_window_map_hook   = nullptr;
        CFunctionHook*         g_window_state_hook = nullptr;

        using OnWindowCreatedTilingFn = void (*)(void*, PHLWINDOW, eDirection);
        using OnWindowMapFn           = void (*)(void*);
        using OnWindowUpdateStateFn   = void (*)(void*);

        void hk_dwindle_on_window_created_tiling(void* thisptr, PHLWINDOW window, eDirection direction) {
            if (g_dwindle_hook && g_dwindle_hook->m_original) {
                (reinterpret_cast<OnWindowCreatedTilingFn>(g_dwindle_hook->m_original))(thisptr, window, direction);
            }
            if (g_active_server) {
                g_active_server->maybe_restore_layout(window);
            }
        }

        void hk_master_on_window_created_tiling(void* thisptr, PHLWINDOW window, eDirection direction) {
            if (g_master_hook && g_master_hook->m_original) {
                (reinterpret_cast<OnWindowCreatedTilingFn>(g_master_hook->m_original))(thisptr, window, direction);
            }
            if (g_active_server) {
                g_active_server->maybe_restore_layout(window);
            }
        }

        void hk_window_on_map(void* thisptr) {
            if (g_window_map_hook && g_window_map_hook->m_original) {
                (reinterpret_cast<OnWindowMapFn>(g_window_map_hook->m_original))(thisptr);
            }
            if (!g_active_server || !thisptr) {
                return;
            }
            auto* window_ptr = reinterpret_cast<Desktop::View::CWindow*>(thisptr);
            if (!window_ptr) {
                return;
            }
            const auto window = window_ptr->m_self.lock();
            if (!window) {
                return;
            }
            g_active_server->maybe_restore_window_state(window);
            g_active_server->record_window_state(window);
        }

        void hk_window_on_update_state(void* thisptr) {
            if (g_window_state_hook && g_window_state_hook->m_original) {
                (reinterpret_cast<OnWindowUpdateStateFn>(g_window_state_hook->m_original))(thisptr);
            }
            if (!g_active_server || !thisptr) {
                return;
            }
            auto* window_ptr = reinterpret_cast<Desktop::View::CWindow*>(thisptr);
            if (!window_ptr) {
                return;
            }
            const auto window = window_ptr->m_self.lock();
            if (!window) {
                return;
            }
            g_active_server->record_window_state(window);
        }

        SessionReason reason_from_protocol(uint32_t reason) {
            switch (reason) {
                case XX_SESSION_MANAGER_V1_REASON_LAUNCH: return SessionReason::kLaunch;
                case XX_SESSION_MANAGER_V1_REASON_RECOVER: return SessionReason::kRecover;
                case XX_SESSION_MANAGER_V1_REASON_SESSION_RESTORE: return SessionReason::kSessionRestore;
                default: return SessionReason::kSessionRestore;
            }
        }

        const SessionProtocolServer::ToplevelResource* find_restored_toplevel(const std::vector<std::unique_ptr<SessionProtocolServer::ToplevelResource>>& toplevels,
                                                                              const PHLWINDOW&                                                             window) {
            if (!window) {
                return nullptr;
            }
            for (const auto& entry : toplevels) {
                if (!entry || entry->inert || !entry->restored) {
                    continue;
                }
                if (entry->name.empty() || entry->session_id.empty()) {
                    continue;
                }
                const auto xdg = CXDGToplevelResource::fromResource(entry->toplevel);
                if (!xdg) {
                    continue;
                }
                const auto target = xdg->m_window.lock();
                if (target && target == window) {
                    return entry.get();
                }
            }
            return nullptr;
        }

        const SessionProtocolServer::ToplevelResource* find_toplevel_for_window(const std::vector<std::unique_ptr<SessionProtocolServer::ToplevelResource>>& toplevels,
                                                                                const PHLWINDOW&                                                             window) {
            if (!window) {
                return nullptr;
            }
            for (const auto& entry : toplevels) {
                if (!entry || entry->inert) {
                    continue;
                }
                if (entry->name.empty() || entry->session_id.empty()) {
                    continue;
                }
                const auto xdg = CXDGToplevelResource::fromResource(entry->toplevel);
                if (!xdg) {
                    continue;
                }
                const auto target = xdg->m_window.lock();
                if (target && target == window) {
                    return entry.get();
                }
            }
            return nullptr;
        }

        void set_window_floating(const PHLWINDOW& window, bool floating) {
            if (!window || !g_pLayoutManager) {
                return;
            }
            if (window->m_isFloating == floating) {
                return;
            }

            if (window->m_groupData.pNextWindow.lock() && window->m_groupData.pNextWindow.lock() != window) {
                const auto current = window->getGroupCurrent();
                if (!current) {
                    return;
                }
                current->m_isFloating = floating;
                g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(current);

                PHLWINDOW curr = current->m_groupData.pNextWindow.lock();
                while (curr && curr != current) {
                    curr->m_isFloating = floating;
                    curr               = curr->m_groupData.pNextWindow.lock();
                }
            } else {
                window->m_isFloating = floating;
                g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(window);
            }

            if (window->m_workspace) {
                window->m_workspace->updateWindows();
                window->m_workspace->updateWindowData();
            }
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(window->monitorID());
            if (g_pCompositor) {
                g_pCompositor->updateAllWindowsAnimatedDecorationValues();
            }
        }

        void apply_window_geometry(const PHLWINDOW& window, const WindowGeometry& geometry) {
            if (!window) {
                return;
            }
            const Vector2D pos{static_cast<double>(geometry.x), static_cast<double>(geometry.y)};
            const Vector2D size{static_cast<double>(geometry.width), static_cast<double>(geometry.height)};
            window->m_realPosition->setValueAndWarp(pos);
            window->m_realSize->setValueAndWarp(size);
            window->m_position             = window->m_realPosition->goal();
            window->m_size                 = window->m_realSize->goal();
            window->m_lastFloatingPosition = pos;
            window->m_lastFloatingSize     = size;
            window->sendWindowSize();
        }

        void apply_window_fullscreen(const PHLWINDOW& window, const FullscreenState& fullscreen) {
            if (!window || !g_pCompositor) {
                return;
            }
            g_pCompositor->setWindowFullscreenState(window,
                                                    Desktop::View::SFullscreenState{
                                                        .internal = static_cast<eFullscreenMode>(fullscreen.internal),
                                                        .client   = static_cast<eFullscreenMode>(fullscreen.client),
                                                    });
        }

        void apply_window_pinned(const PHLWINDOW& window, bool pinned) {
            if (!window) {
                return;
            }
            if (!window->m_isFloating || window->isFullscreen()) {
                return;
            }
            if (window->m_pinned == pinned) {
                return;
            }
            window->m_pinned = pinned;
            if (window->m_ruleApplicator) {
                window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_PINNED);
            }
            if (window->m_workspace) {
                window->m_workspace->updateWindows();
                window->m_workspace->updateWindowData();
            }
            if (g_pLayoutManager) {
                g_pLayoutManager->getCurrentLayout()->recalculateMonitor(window->monitorID());
            }
            if (g_pCompositor) {
                g_pCompositor->updateAllWindowsAnimatedDecorationValues();
            }
        }

        void apply_toplevel_state(const PHLWINDOW& window, const SessionProtocolStore::ToplevelState& state) {
            if (state.floating.has_value()) {
                set_window_floating(window, *state.floating);
            }
            if (state.geometry && window && window->m_isFloating) {
                apply_window_geometry(window, *state.geometry);
            }
            if (state.fullscreen) {
                apply_window_fullscreen(window, *state.fullscreen);
            }
            if (state.pinned.has_value()) {
                apply_window_pinned(window, *state.pinned);
            }
        }

        const struct xx_session_manager_v1_interface kSessionManagerImpl = {
            .destroy     = session_manager_destroy,
            .get_session = session_manager_get_session,
        };

        const struct xx_session_v1_interface kSessionImpl = {
            .destroy          = session_destroy,
            .remove           = session_remove,
            .add_toplevel     = session_add_toplevel,
            .restore_toplevel = session_restore_toplevel,
        };

        const struct xx_toplevel_session_v1_interface kToplevelImpl = {
            .destroy = toplevel_destroy,
            .remove  = toplevel_remove,
        };

        bool toplevel_is_mapped(wl_resource* toplevel) {
            const auto xdg = CXDGToplevelResource::fromResource(toplevel);
            if (!xdg) {
                return false;
            }
            const auto window = xdg->m_window.lock();
            if (window && window->m_isMapped) {
                return true;
            }
            const auto surface = xdg->m_owner.lock();
            return surface && surface->m_mapped;
        }

        bool toplevel_has_initial_commit(wl_resource* toplevel) {
            const auto xdg = CXDGToplevelResource::fromResource(toplevel);
            if (!xdg) {
                return false;
            }
            const auto surface = xdg->m_owner.lock();
            if (!surface) {
                return false;
            }
            return !surface->m_initialCommit;
        }

        SessionProtocolServer* server_from_manager(wl_resource* resource) {
            return resource ? static_cast<SessionProtocolServer*>(wl_resource_get_user_data(resource)) : nullptr;
        }

        SessionProtocolServer::SessionResource* session_from_resource(wl_resource* resource) {
            return resource ? static_cast<SessionProtocolServer::SessionResource*>(wl_resource_get_user_data(resource)) : nullptr;
        }

        SessionProtocolServer::ToplevelResource* toplevel_from_resource(wl_resource* resource) {
            return resource ? static_cast<SessionProtocolServer::ToplevelResource*>(wl_resource_get_user_data(resource)) : nullptr;
        }

        void session_manager_destroy(wl_client*, wl_resource* resource) {
            if (resource) {
                wl_resource_destroy(resource);
            }
        }

        void session_manager_get_session(wl_client* client, wl_resource* resource, uint32_t id, uint32_t reason, const char* session) {
            auto* server = server_from_manager(resource);
            if (!server) {
                return;
            }
            std::optional<std::string_view> requested;
            if (session && *session != '\0') {
                requested = session;
            }
            std::string error;
            const auto  open = server->open_session(client, requested, reason_from_protocol(reason), &error);
            if (!open) {
                if (error == "session in use") {
                    wl_resource_post_error(resource, XX_SESSION_MANAGER_V1_ERROR_IN_USE, "%s", error.c_str());
                }
                return;
            }

            if (open->state == SessionOpenState::kReplaced) {
                server->mark_session_replaced(open->id);
            }

            wl_resource* session_resource = wl_resource_create(client, &xx_session_v1_interface, wl_resource_get_version(resource), id);
            if (!session_resource) {
                wl_client_post_no_memory(client);
                return;
            }

            auto entry      = std::make_unique<SessionProtocolServer::SessionResource>();
            entry->server   = server;
            entry->resource = session_resource;
            entry->client   = client;
            entry->id       = open->id;

            wl_resource_set_implementation(session_resource, &kSessionImpl, entry.get(), destroy_session_resource);
            server->register_session(std::move(entry));

            if (open->state == SessionOpenState::kCreated) {
                xx_session_v1_send_created(session_resource, open->id.c_str());
            } else {
                xx_session_v1_send_restored(session_resource);
            }
        }

        void session_destroy(wl_client*, wl_resource* resource) {
            auto* session = session_from_resource(resource);
            if (!session || !session->server) {
                return;
            }
            session->server->handle_session_destroy(session, false);
        }

        void session_remove(wl_client*, wl_resource* resource) {
            auto* session = session_from_resource(resource);
            if (!session || !session->server) {
                return;
            }
            session->server->handle_session_destroy(session, true);
        }

        void session_add_toplevel(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* toplevel, const char* name) {
            auto* session = session_from_resource(resource);
            if (!session || !session->server || session->inert) {
                return;
            }
            if (!toplevel || !name || *name == '\0') {
                wl_resource_post_error(resource, XX_SESSION_V1_ERROR_NAME_IN_USE, "invalid toplevel name");
                return;
            }

            std::string error;
            if (!session->server->add_toplevel(session->id, name, &error)) {
                const auto message = error.empty() ? std::string("toplevel name in use") : error;
                wl_resource_post_error(resource, XX_SESSION_V1_ERROR_NAME_IN_USE, "%s", message.c_str());
                return;
            }

            wl_resource* toplevel_resource = wl_resource_create(client, &xx_toplevel_session_v1_interface, wl_resource_get_version(resource), id);
            if (!toplevel_resource) {
                wl_client_post_no_memory(client);
                return;
            }

            auto entry        = std::make_unique<SessionProtocolServer::ToplevelResource>();
            entry->server     = session->server;
            entry->resource   = toplevel_resource;
            entry->toplevel   = toplevel;
            entry->session_id = session->id;
            entry->name       = name;

            wl_resource_set_implementation(toplevel_resource, &kToplevelImpl, entry.get(), destroy_toplevel_resource);
            session->server->register_toplevel(std::move(entry));
        }

        void session_restore_toplevel(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* toplevel, const char* name) {
            auto* session = session_from_resource(resource);
            if (!session || !session->server || session->inert) {
                return;
            }
            if (!toplevel || !name || *name == '\0') {
                wl_resource_post_error(resource, XX_SESSION_V1_ERROR_NAME_IN_USE, "invalid toplevel name");
                return;
            }
            if (toplevel_is_mapped(toplevel)) {
                wl_resource_post_error(resource, XX_SESSION_V1_ERROR_ALREADY_MAPPED, "toplevel already mapped");
                return;
            }
            if (toplevel_has_initial_commit(toplevel)) {
                wl_resource_post_error(resource, XX_SESSION_V1_ERROR_INVALID_RESTORE, "restore after initial commit");
                return;
            }

            std::string error;
            if (!session->server->add_toplevel(session->id, name, &error)) {
                const auto message = error.empty() ? std::string("toplevel name in use") : error;
                wl_resource_post_error(resource, XX_SESSION_V1_ERROR_NAME_IN_USE, "%s", message.c_str());
                return;
            }

            wl_resource* toplevel_resource = wl_resource_create(client, &xx_toplevel_session_v1_interface, wl_resource_get_version(resource), id);
            if (!toplevel_resource) {
                wl_client_post_no_memory(client);
                return;
            }

            auto entry        = std::make_unique<SessionProtocolServer::ToplevelResource>();
            entry->server     = session->server;
            entry->resource   = toplevel_resource;
            entry->toplevel   = toplevel;
            entry->session_id = session->id;
            entry->name       = name;
            entry->restored   = true;

            wl_resource_set_implementation(toplevel_resource, &kToplevelImpl, entry.get(), destroy_toplevel_resource);
            session->server->register_toplevel(std::move(entry));
            xx_toplevel_session_v1_send_restored(toplevel_resource, toplevel);
        }

        void toplevel_destroy(wl_client*, wl_resource* resource) {
            auto* toplevel = toplevel_from_resource(resource);
            if (!toplevel || !toplevel->server) {
                return;
            }
            toplevel->server->handle_toplevel_destroy(toplevel, false);
        }

        void toplevel_remove(wl_client*, wl_resource* resource) {
            auto* toplevel = toplevel_from_resource(resource);
            if (!toplevel || !toplevel->server) {
                return;
            }
            toplevel->server->handle_toplevel_destroy(toplevel, true);
        }

        void destroy_session_resource(wl_resource* resource) {
            auto* session = session_from_resource(resource);
            if (!session || !session->server) {
                return;
            }
            session->server->on_session_resource_destroy(session);
        }

        void destroy_toplevel_resource(wl_resource* resource) {
            auto* toplevel = toplevel_from_resource(resource);
            if (!toplevel || !toplevel->server) {
                return;
            }
            toplevel->server->on_toplevel_resource_destroy(toplevel);
        }

    } // namespace

    SessionProtocolServer::SessionProtocolServer(std::filesystem::path store_path, SessionIdGenerator generator) :
        IWaylandProtocol(&xx_session_manager_v1_interface, 1, "xx_session_manager_v1"), state_(std::move(generator)), store_path_(std::move(store_path)) {
        g_active_server = this;
        std::string error;
        if (!state_.load_store(store_path_, &error) && !error.empty()) {
            LOGM(Log::ERR, "Failed to load session store: {}", error);
        }
    }

    SessionProtocolServer::~SessionProtocolServer() {
        if (g_active_server == this) {
            g_active_server = nullptr;
        }
        if (hook_handle_) {
            for (auto* hook : layout_hooks_) {
                if (hook) {
                    HyprlandAPI::removeFunctionHook(hook_handle_, hook);
                }
            }
        }
    }

    void SessionProtocolServer::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
        (void)data;
        wl_resource* manager_resource = wl_resource_create(client, &xx_session_manager_v1_interface, ver, id);
        if (!manager_resource) {
            wl_client_post_no_memory(client);
            return;
        }
        wl_resource_set_implementation(manager_resource, &kSessionManagerImpl, this, nullptr);
    }

    void SessionProtocolServer::install_layout_hooks(void* handle) {
        if (!handle || hook_handle_) {
            return;
        }
#if !defined(__x86_64__)
        LOGM(Log::ERR, "Function hooks require x86_64");
        return;
#endif
        hook_handle_              = handle;
        const auto layout_matches = HyprlandAPI::findFunctionsByName(handle, "onWindowCreatedTiling");
        if (layout_matches.empty()) {
            LOGM(Log::ERR, "Failed to locate onWindowCreatedTiling for layout hooks");
            return;
        }

        const auto find_match = [&](const std::vector<SFunctionMatch>& matches, std::string_view class_tag) -> std::optional<SFunctionMatch> {
            for (const auto& match : matches) {
                if (match.demangled.find(class_tag) != std::string::npos || match.signature.find(class_tag) != std::string::npos) {
                    return match;
                }
            }
            return std::nullopt;
        };

        auto dwindle = find_match(layout_matches, "CHyprDwindleLayout");
        if (!dwindle) {
            const auto fallback = HyprlandAPI::findFunctionsByName(handle, "CHyprDwindleLayout::onWindowCreatedTiling");
            if (!fallback.empty()) {
                dwindle = fallback.front();
            }
        }

        auto master = find_match(layout_matches, "CHyprMasterLayout");
        if (!master) {
            const auto fallback = HyprlandAPI::findFunctionsByName(handle, "CHyprMasterLayout::onWindowCreatedTiling");
            if (!fallback.empty()) {
                master = fallback.front();
            }
        }

        if (dwindle) {
            g_dwindle_hook = HyprlandAPI::createFunctionHook(handle, dwindle->address, (void*)hk_dwindle_on_window_created_tiling);
            if (g_dwindle_hook && g_dwindle_hook->hook()) {
                layout_hooks_.push_back(g_dwindle_hook);
            } else {
                LOGM(Log::ERR, "Failed to hook CHyprDwindleLayout::onWindowCreatedTiling");
            }
        }

        if (master) {
            g_master_hook = HyprlandAPI::createFunctionHook(handle, master->address, (void*)hk_master_on_window_created_tiling);
            if (g_master_hook && g_master_hook->hook()) {
                layout_hooks_.push_back(g_master_hook);
            } else {
                LOGM(Log::ERR, "Failed to hook CHyprMasterLayout::onWindowCreatedTiling");
            }
        }

        const auto map_matches = HyprlandAPI::findFunctionsByName(handle, "onMap");
        if (map_matches.empty()) {
            LOGM(Log::ERR, "Failed to locate onMap for window hooks");
            return;
        }

        auto map_match = find_match(map_matches, "CWindow");
        if (!map_match) {
            const auto fallback = HyprlandAPI::findFunctionsByName(handle, "CWindow::onMap");
            if (!fallback.empty()) {
                map_match = fallback.front();
            }
        }

        if (map_match) {
            g_window_map_hook = HyprlandAPI::createFunctionHook(handle, map_match->address, (void*)hk_window_on_map);
            if (g_window_map_hook && g_window_map_hook->hook()) {
                layout_hooks_.push_back(g_window_map_hook);
            } else {
                LOGM(Log::ERR, "Failed to hook CWindow::onMap");
            }
        }

        const auto state_matches = HyprlandAPI::findFunctionsByName(handle, "onUpdateState");
        if (state_matches.empty()) {
            LOGM(Log::ERR, "Failed to locate onUpdateState for window hooks");
            return;
        }

        auto state_match = find_match(state_matches, "CWindow");
        if (!state_match) {
            const auto fallback = HyprlandAPI::findFunctionsByName(handle, "CWindow::onUpdateState");
            if (!fallback.empty()) {
                state_match = fallback.front();
            }
        }

        if (state_match) {
            g_window_state_hook = HyprlandAPI::createFunctionHook(handle, state_match->address, (void*)hk_window_on_update_state);
            if (g_window_state_hook && g_window_state_hook->hook()) {
                layout_hooks_.push_back(g_window_state_hook);
            } else {
                LOGM(Log::ERR, "Failed to hook CWindow::onUpdateState");
            }
        }
    }

    std::optional<std::string> SessionProtocolServer::save(std::string_view) {
        std::unordered_map<const void*, std::string> window_names;
        std::unordered_set<std::string>              session_ids;
        for (const auto& entry : toplevels_) {
            if (!entry || entry->inert || entry->name.empty()) {
                continue;
            }
            const auto xdg = CXDGToplevelResource::fromResource(entry->toplevel);
            if (!xdg) {
                continue;
            }
            const auto window = xdg->m_window.lock();
            if (!window) {
                continue;
            }

            window_names[window.get()] = entry->name;
            session_ids.insert(entry->session_id);

            SessionProtocolStore::ToplevelState state{
                .name = entry->name,
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
            };

            state_.update_toplevel_state(entry->session_id, entry->name, state);
        }

        if (!session_ids.empty()) {
            std::string layout_error;
            const auto  layout_snapshot = layout_capture::capture_layout_snapshot(window_names, &layout_error);
            if (!layout_snapshot) {
                return layout_error.empty() ? std::optional<std::string>("failed to capture layout snapshot") : std::optional<std::string>(layout_error);
            }
            for (const auto& id : session_ids) {
                state_.update_layout_snapshot(id, *layout_snapshot);
            }
        }

        std::string error;
        if (!state_.save_store(store_path_, &error)) {
            return error.empty() ? std::optional<std::string>("failed to write session store") : std::optional<std::string>(error);
        }
        return std::nullopt;
    }

    std::optional<std::string> SessionProtocolServer::restore(std::string_view) {
        std::string error;
        if (!state_.load_store(store_path_, &error)) {
            return error.empty() ? std::optional<std::string>("failed to load session store") : std::optional<std::string>(error);
        }
        layout_tracker_.clear();
        toplevel_state_tracker_.clear();
        for (const auto& id : state_.store().session_ids()) {
            if (const auto toplevels = state_.store().toplevels_for(id)) {
                toplevel_state_tracker_.set_states(id, *toplevels);
            }
            const auto snapshot = state_.store().layout_for(id);
            if (!snapshot || snapshot->kind == LayoutKind::kUnknown) {
                continue;
            }
            layout_tracker_.set_snapshot(id, *snapshot);
        }
        return std::nullopt;
    }

    void SessionProtocolServer::handle_session_destroy(SessionResource* session, bool remove) {
        if (!session || session->inert) {
            return;
        }
        session->inert = true;
        if (remove) {
            state_.remove_session(session->id);
        } else {
            state_.close_session(session->client, session->id);
        }
        layout_tracker_.clear_session(session->id);
        toplevel_state_tracker_.clear_session(session->id);
        std::vector<wl_resource*> to_destroy;
        for (const auto& entry : toplevels_) {
            if (!entry || entry->session_id != session->id) {
                continue;
            }
            entry->inert = true;
            if (remove) {
                to_destroy.push_back(entry->resource);
            }
        }
        for (auto* resource : to_destroy) {
            wl_resource_destroy(resource);
        }
        wl_resource_destroy(session->resource);
    }

    void SessionProtocolServer::handle_toplevel_destroy(ToplevelResource* toplevel, bool remove) {
        if (!toplevel || toplevel->inert) {
            return;
        }
        toplevel->inert = true;
        if (remove) {
            state_.remove_toplevel(toplevel->session_id, toplevel->name);
        } else {
            state_.release_toplevel(toplevel->session_id, toplevel->name);
        }
        wl_resource_destroy(toplevel->resource);
    }

    void SessionProtocolServer::mark_session_replaced(std::string_view id) {
        layout_tracker_.clear_session(id);
        toplevel_state_tracker_.clear_session(id);
        for (const auto& entry : sessions_) {
            if (!entry || entry->id != id) {
                continue;
            }
            if (!entry->inert) {
                entry->inert = true;
                xx_session_v1_send_replaced(entry->resource);
            }
        }
        for (const auto& entry : toplevels_) {
            if (!entry || entry->session_id != id) {
                continue;
            }
            entry->inert = true;
        }
    }

    void SessionProtocolServer::on_session_resource_destroy(SessionResource* session) {
        if (!session) {
            return;
        }
        if (!session->inert) {
            state_.close_session(session->client, session->id);
        }
        std::erase_if(sessions_, [&](const auto& entry) { return entry.get() == session; });
    }

    void SessionProtocolServer::on_toplevel_resource_destroy(ToplevelResource* toplevel) {
        if (!toplevel) {
            return;
        }
        std::erase_if(toplevels_, [&](const auto& entry) { return entry.get() == toplevel; });
    }

    void SessionProtocolServer::register_session(std::unique_ptr<SessionResource> session) {
        sessions_.push_back(std::move(session));
    }

    void SessionProtocolServer::register_toplevel(std::unique_ptr<ToplevelResource> toplevel) {
        toplevels_.push_back(std::move(toplevel));
    }

    bool SessionProtocolServer::add_toplevel(std::string_view session_id, std::string_view name, std::string* error) {
        return state_.add_toplevel(session_id, name, error);
    }

    bool SessionProtocolServer::remove_toplevel(std::string_view session_id, std::string_view name) {
        return state_.remove_toplevel(session_id, name);
    }

    std::optional<SessionOpenResult> SessionProtocolServer::open_session(void* client, std::optional<std::string_view> requested_id, SessionReason reason, std::string* error) {
        return state_.open_session(client, requested_id, reason, error);
    }

    void SessionProtocolServer::maybe_restore_layout(const PHLWINDOW& window) {
        if (!window) {
            return;
        }
        const auto* entry = find_restored_toplevel(toplevels_, window);
        if (!entry) {
            return;
        }

        auto ready = layout_tracker_.record_window(entry->session_id, entry->name, window);
        if (!ready) {
            return;
        }
        if (const auto error = apply_layout_snapshot(ready->snapshot, ready->windows)) {
            LOGM(Log::ERR, "Failed to restore layout: {}", *error);
        }
    }

    void SessionProtocolServer::maybe_restore_window_state(const PHLWINDOW& window) {
        if (!window) {
            return;
        }
        const auto* entry = find_restored_toplevel(toplevels_, window);
        if (!entry) {
            return;
        }
        auto state = toplevel_state_tracker_.take_state(entry->session_id, entry->name);
        if (!state) {
            return;
        }
        apply_toplevel_state(window, *state);
    }

    void SessionProtocolServer::record_window_state(const PHLWINDOW& window) {
        if (!window) {
            return;
        }
        const auto* entry = find_toplevel_for_window(toplevels_, window);
        if (!entry) {
            return;
        }

        SessionProtocolStore::ToplevelState state{
            .name = entry->name,
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
        };

        state_.update_toplevel_state(entry->session_id, entry->name, state);
    }

} // namespace hyprspaces
