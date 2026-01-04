#include <gtest/gtest.h>

#include "hyprspaces/runtime_utils.hpp"

TEST(RuntimeUtils, BuildsPrefixedCommand) {
    EXPECT_EQ(hyprspaces::build_prefixed_command("session save", ""), "session save");
    EXPECT_EQ(hyprspaces::build_prefixed_command("session save", "   "), "session save");
    EXPECT_EQ(hyprspaces::build_prefixed_command("paired switch", "  2 "), "paired switch 2");
}

TEST(RuntimeUtils, ResolvesWaybarThemePath) {
    const hyprspaces::Paths paths{
        .base_dir             = "/tmp/hyprspaces",
        .hyprspaces_conf_path = "/tmp/hyprspaces/hyprspaces.conf",
        .hyprland_conf_path   = "/tmp/hypr/hyprland.conf",
        .profiles_dir         = "/tmp/hypr/hyprspaces/profiles",
        .proc_root            = "/proc",
        .state_dir            = "/tmp/state/hyprspaces",
        .session_store_path   = "/tmp/state/hyprspaces/session-store.json",
        .waybar_dir           = "/tmp/hyprspaces/waybar",
        .waybar_theme_css     = "/tmp/hyprspaces/waybar/theme.css",
    };
    hyprspaces::Config config{
        .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0}},
        .workspace_count       = 10,
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
    const hyprspaces::RuntimeConfig runtime_config{
        .paths  = paths,
        .config = config,
    };

    EXPECT_EQ(hyprspaces::resolve_waybar_theme_path(runtime_config, std::nullopt), std::filesystem::path("/tmp/hyprspaces/waybar/theme.css"));
    EXPECT_EQ(hyprspaces::resolve_waybar_theme_path(runtime_config, std::string("/tmp/override.css")), std::filesystem::path("/tmp/override.css"));

    config.waybar_theme_css = std::string("/tmp/config.css");
    const hyprspaces::RuntimeConfig config_override{
        .paths  = paths,
        .config = config,
    };
    EXPECT_EQ(hyprspaces::resolve_waybar_theme_path(config_override, std::nullopt), std::filesystem::path("/tmp/config.css"));
}
