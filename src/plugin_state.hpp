#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/debounce.hpp"
#include "hyprspaces/history.hpp"
#include "hyprspaces/marks.hpp"
#include "hyprspaces/monitor_profiles.hpp"
#include "hyprspaces/profiles.hpp"
#include "hyprspaces/runtime.hpp"
#include "hyprspaces/waybar.hpp"
#include "hyprspaces/waybar_socket.hpp"
#include "session_protocol_server.hpp"

struct wl_event_source;

struct ConfigPointers {
    Hyprlang::CConfigValue* workspace_count     = nullptr;
    Hyprlang::CConfigValue* wrap_cycling        = nullptr;
    Hyprlang::CConfigValue* safe_mode           = nullptr;
    Hyprlang::CConfigValue* debug_logging       = nullptr;
    Hyprlang::CConfigValue* waybar_enabled      = nullptr;
    Hyprlang::CConfigValue* waybar_theme_css    = nullptr;
    Hyprlang::CConfigValue* waybar_class        = nullptr;
    Hyprlang::CConfigValue* notify_session      = nullptr;
    Hyprlang::CConfigValue* notify_timeout_ms   = nullptr;
    Hyprlang::CConfigValue* session_enabled     = nullptr;
    Hyprlang::CConfigValue* session_autosave    = nullptr;
    Hyprlang::CConfigValue* session_autorestore = nullptr;
    Hyprlang::CConfigValue* session_reason      = nullptr;
    Hyprlang::CConfigValue* persistent_all      = nullptr;
};

class WaybarBridge {
  public:
    explicit WaybarBridge(std::filesystem::path socket_path);

    std::optional<std::string> start();
    void                       stop();
    bool                       running() const;
    void                       broadcast(std::string_view message);

  private:
    static int                     on_server_fd(int fd, uint32_t mask, void* data);
    void                           accept_ready();

    hyprspaces::WaybarSocketServer server_;
    wl_event_source*               event_source_ = nullptr;
};

struct PluginState {
    hyprspaces::Paths                                  paths;
    ConfigPointers                                     config;
    std::optional<hyprspaces::RuntimeConfigCache>      runtime_cache;
    bool                                               enabled = true;
    hyprspaces::RebalanceDebounce                      rebalance_debounce{std::chrono::milliseconds(200)};
    hyprspaces::FocusSwitchDebounce                    focus_debounce{std::chrono::milliseconds(100)};
    hyprspaces::PendingDebounce                        waybar_debounce{std::chrono::milliseconds(100)};
    hyprspaces::WorkspaceHistory                       workspace_history;
    std::optional<WaybarBridge>                        waybar_bridge;
    hyprspaces::WaybarThemeCache                       waybar_theme_cache;
    std::unique_ptr<hyprspaces::SessionProtocolServer> session_protocol;
    std::optional<hyprspaces::SessionCoordinator>      session_coordinator;
    std::vector<hyprspaces::MonitorGroup>              monitor_groups;
    std::vector<hyprspaces::MonitorProfile>            monitor_profiles;
    std::vector<int>                                   persistent_workspaces;
    hyprspaces::ProfileCatalog                         profile_catalog;
    hyprspaces::MarksStore                             marks;
    std::vector<SP<HOOK_CALLBACK_FN>>                  hooks;
};

class HyprlandInvoker : public hyprspaces::HyprctlInvoker {
  public:
    std::string invoke(std::string_view call, std::string_view args, std::string_view format) override;
};

extern HANDLE          PHANDLE;
extern PluginState     g_state;
extern HyprlandInvoker g_invoker;
void                   fail_fast(std::string_view context, std::string_view message);
