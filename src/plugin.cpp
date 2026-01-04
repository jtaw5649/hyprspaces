#include "hyprspaces/plugin_entry.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    const auto result = hyprspaces::plugin::init(handle);
    return PLUGIN_DESCRIPTION_INFO{result.name, result.description, result.author, result.version};
}

APICALL EXPORT void PLUGIN_EXIT() {
    hyprspaces::plugin::exit();
}
