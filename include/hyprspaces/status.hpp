#ifndef HYPRSPACES_STATUS_HPP
#define HYPRSPACES_STATUS_HPP

#include <string>
#include <string_view>

#include "hyprspaces/config.hpp"

namespace hyprspaces {

    std::string render_status(const Config& config, int active_workspace_id);
    std::string render_status_json(const Config& config, int active_workspace_id);

} // namespace hyprspaces

#endif // HYPRSPACES_STATUS_HPP
