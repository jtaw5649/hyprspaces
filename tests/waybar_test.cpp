#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

#include "hyprspaces/config.hpp"
#include "hyprspaces/types.hpp"
#include "hyprspaces/waybar.hpp"

namespace {

    std::string read_fixture(const std::string& name) {
        const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / "fixtures" / name;
        std::ifstream               input(path);
        std::ostringstream          buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

} // namespace

TEST(WaybarRender, MatchesFixture) {
    const hyprspaces::Config config{
        .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"HDMI-A-1", "DP-1"}, .workspace_offset = 0}},
        .workspace_count       = 5,
        .wrap_cycling          = true,
        .safe_mode             = true,
        .debug_logging         = false,
        .waybar_enabled        = true,
        .waybar_theme_css      = std::nullopt,
        .waybar_class          = std::nullopt,
        .session_enabled       = false,
        .session_autosave      = false,
        .session_autorestore   = false,
        .session_reason        = hyprspaces::SessionReason::kSessionRestore,
        .persistent_all        = true,
        .persistent_workspaces = {},
    };
    const std::vector<hyprspaces::WorkspaceInfo> workspaces = {
        {.id = 1, .windows = 2, .name = std::string("1"), .monitor = std::string("HDMI-A-1")},
        {.id = 2, .windows = 1, .name = std::string("2"), .monitor = std::string("HDMI-A-1")},
        {.id = 3, .windows = 1, .name = std::string("3"), .monitor = std::string("DP-1")},
    };
    const std::vector<hyprspaces::MonitorInfo> monitors = {
        {.name = "HDMI-A-1", .id = 1, .x = 0, .description = std::nullopt, .active_workspace_id = 2},
        {.name = "DP-1", .id = 2, .x = 1920, .description = std::nullopt, .active_workspace_id = 3},
    };
    const std::vector<hyprspaces::ClientInfo> clients = {
        {.address       = "0x1",
         .workspace     = {.id = 1, .name = std::string("1")},
         .class_name    = std::nullopt,
         .title         = std::nullopt,
         .initial_class = std::nullopt,
         .initial_title = std::nullopt,
         .app_id        = std::nullopt,
         .pid           = std::nullopt,
         .command       = std::nullopt,
         .geometry      = std::nullopt,
         .floating      = std::nullopt,
         .pinned        = std::nullopt,
         .fullscreen    = std::nullopt,
         .urgent        = true},
    };

    const auto theme = hyprspaces::parse_waybar_theme(read_fixture("theme.css"));
    ASSERT_TRUE(theme.has_value());

    const auto output = hyprspaces::render_waybar_json(config, 2, workspaces, monitors, clients, *theme);

    EXPECT_EQ(output, read_fixture("waybar.json"));
}

TEST(WaybarThemeCache, ReloadsWhenThemeChanges) {
    const auto theme_path = std::filesystem::temp_directory_path() / "hyprspaces-theme.css";
    {
        std::ofstream output(theme_path);
        output << "@define-color foreground #112233;";
    }
    const auto initial_time = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(theme_path, initial_time);

    hyprspaces::WaybarThemeCache cache;
    std::string                  error;
    const auto                   first = cache.load(theme_path, &error);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->foreground, "#112233");

    {
        std::ofstream output(theme_path);
        output << "@define-color foreground #445566;";
    }
    std::filesystem::last_write_time(theme_path, initial_time + std::chrono::seconds(1));

    const auto second = cache.load(theme_path, &error);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->foreground, "#445566");

    std::filesystem::remove(theme_path);
}

TEST(WaybarThemeCache, FallsBackToDefaultWhenMissing) {
    const auto      theme_path = std::filesystem::temp_directory_path() / "hyprspaces-missing-theme.css";
    std::error_code ec;
    std::filesystem::remove(theme_path, ec);

    hyprspaces::WaybarThemeCache cache;
    std::string                  error;
    const auto                   theme = cache.load(theme_path, &error);
    ASSERT_TRUE(theme.has_value());
    EXPECT_TRUE(error.empty());

    const auto expected = hyprspaces::parse_waybar_theme(read_fixture("theme.css"));
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(theme->foreground, expected->foreground);
    EXPECT_EQ(theme->mid, expected->mid);
    EXPECT_EQ(theme->dim, expected->dim);
}
