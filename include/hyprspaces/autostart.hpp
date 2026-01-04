#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hyprspaces {

    bool                               autostart_hidden(std::string_view app_id, const std::vector<std::filesystem::path>& autostart_dirs);

    std::vector<std::filesystem::path> autostart_paths_from_env();

} // namespace hyprspaces
