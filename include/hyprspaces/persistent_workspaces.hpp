#ifndef HYPRSPACES_PERSISTENT_WORKSPACES_HPP
#define HYPRSPACES_PERSISTENT_WORKSPACES_HPP

#include <optional>
#include <string_view>

namespace hyprspaces {

    std::optional<int> parse_persistent_workspace(std::string_view value);

} // namespace hyprspaces

#endif // HYPRSPACES_PERSISTENT_WORKSPACES_HPP
