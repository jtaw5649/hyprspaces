#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/config.hpp"
#include "hyprspaces/types.hpp"

namespace hyprspaces {

    struct WaybarTheme {
        std::string foreground;
        std::string mid;
        std::string dim;
    };

    std::optional<WaybarTheme> parse_waybar_theme(std::string_view css);

    class WaybarThemeCache {
      public:
        std::optional<WaybarTheme> load(const std::filesystem::path& path, std::string* error = nullptr);

        void                       invalidate();

      private:
        std::filesystem::path                          cached_path_;
        std::optional<std::filesystem::file_time_type> cached_mtime_;
        std::optional<WaybarTheme>                     cached_theme_;
    };

    std::string render_waybar_json(const Config& config, int active_workspace_id, const std::vector<WorkspaceInfo>& workspaces, const std::vector<MonitorInfo>& monitors,
                                   const std::vector<ClientInfo>& clients, const WaybarTheme& theme);

} // namespace hyprspaces
