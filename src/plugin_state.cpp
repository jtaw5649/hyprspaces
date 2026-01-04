#include "plugin_state.hpp"

#include "hyprspaces/failsafe.hpp"

#include <string>

#include <wayland-server-core.h>

#include <hyprland/src/Compositor.hpp>

HANDLE          PHANDLE = nullptr;
PluginState     g_state;
HyprlandInvoker g_invoker;

std::string     HyprlandInvoker::invoke(std::string_view call, std::string_view args, std::string_view format) {
    return HyprlandAPI::invokeHyprctlCommand(std::string(call), std::string(args), std::string(format));
}

WaybarBridge::WaybarBridge(std::filesystem::path socket_path) : server_(std::move(socket_path)) {}

std::optional<std::string> WaybarBridge::start() {
    if (running()) {
        return std::nullopt;
    }
    auto error = server_.start();
    if (error) {
        return error;
    }
    if (!g_pCompositor || !g_pCompositor->m_wlEventLoop) {
        server_.stop();
        return "waybar event loop unavailable";
    }
    event_source_ = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, server_.server_fd(), WL_EVENT_READABLE, WaybarBridge::on_server_fd, this);
    if (!event_source_) {
        server_.stop();
        return "unable to register waybar socket";
    }
    return std::nullopt;
}

void WaybarBridge::stop() {
    if (event_source_) {
        wl_event_source_remove(event_source_);
        event_source_ = nullptr;
    }
    server_.stop();
}

bool WaybarBridge::running() const {
    return server_.running();
}

void WaybarBridge::broadcast(std::string_view message) {
    server_.broadcast(message);
}

int WaybarBridge::on_server_fd(int, uint32_t mask, void* data) {
    auto* self = static_cast<WaybarBridge*>(data);
    if (!self) {
        return 0;
    }
    (void)hyprspaces::failsafe::guard(
        [&]() {
            if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
                self->stop();
                return;
            }
            if (mask & WL_EVENT_READABLE) {
                self->accept_ready();
            }
        },
        fail_fast, "waybar socket");
    return 0;
}

void WaybarBridge::accept_ready() {
    server_.accept_ready();
}
