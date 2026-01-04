#ifndef HYPRSPACES_TYPES_HPP
#define HYPRSPACES_TYPES_HPP

#include <optional>
#include <string>

namespace hyprspaces {

    struct WindowGeometry {
        int x;
        int y;
        int width;
        int height;
    };

    struct FullscreenState {
        int internal = 0;
        int client   = 0;
    };

    struct WorkspaceRef {
        int                        id;
        std::optional<std::string> name;
    };

    struct WorkspaceInfo {
        int                        id;
        int                        windows;
        std::optional<std::string> name;
        std::optional<std::string> monitor;
    };

    struct MonitorInfo {
        std::string                name;
        int                        id;
        int                        x;
        std::optional<std::string> description;
        std::optional<int>         active_workspace_id;
    };

    struct ClientInfo {
        std::string                    address;
        WorkspaceRef                   workspace;
        std::optional<std::string>     class_name;
        std::optional<std::string>     title;
        std::optional<std::string>     initial_class;
        std::optional<std::string>     initial_title;
        std::optional<std::string>     app_id;
        std::optional<int>             pid;
        std::optional<std::string>     command;
        std::optional<WindowGeometry>  geometry;
        std::optional<bool>            floating;
        std::optional<bool>            pinned;
        std::optional<FullscreenState> fullscreen;
        std::optional<bool>            urgent;
    };

} // namespace hyprspaces

#endif // HYPRSPACES_TYPES_HPP
