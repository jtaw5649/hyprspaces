#pragma once

#include <string>

#include <hyprland/src/plugins/PluginAPI.hpp>

namespace hyprspaces::plugin {

    struct PluginInitResult {
        std::string name;
        std::string description;
        std::string author;
        std::string version;
    };

    PluginInitResult init(HANDLE handle);
    void             exit();

} // namespace hyprspaces::plugin
