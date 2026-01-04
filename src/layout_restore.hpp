#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include <hyprland/src/desktop/DesktopTypes.hpp>

#include "hyprspaces/layout_tree.hpp"

namespace hyprspaces {

    std::optional<std::string> apply_layout_snapshot(const LayoutSnapshot& snapshot, const std::unordered_map<std::string, PHLWINDOW>& windows);

} // namespace hyprspaces
