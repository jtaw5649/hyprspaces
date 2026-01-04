#include <filesystem>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "hyprspaces/status.hpp"

namespace {

    std::string read_fixture(const std::string& name) {
        const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / "fixtures" / name;
        std::ifstream               input(path);
        std::ostringstream          buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

} // namespace

TEST(StatusRender, MatchesFixture) {
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

    const auto output = hyprspaces::render_status(config, 7);

    EXPECT_EQ(output, read_fixture("status.txt"));
}

TEST(StatusRender, EmitsJson) {
    const hyprspaces::Config config{
        .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"HDMI-A-1", "DP-1"}, .workspace_offset = 0}},
        .workspace_count       = 5,
        .wrap_cycling          = false,
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

    const auto output = hyprspaces::render_status_json(config, 7);

    const auto json = nlohmann::json::parse(output);
    EXPECT_EQ(json.at("status"), "loaded");
    EXPECT_EQ(json.at("wrap_cycling"), false);
    EXPECT_EQ(json.at("active_group").at("name"), "pair");
    EXPECT_EQ(json.at("active_group").at("normalized"), 2);
    EXPECT_EQ(json.at("active_group").at("workspace_id"), 7);
}
