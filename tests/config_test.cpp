#include <gtest/gtest.h>

#include "hyprspaces/config.hpp"

TEST(ConfigParse, ParsesSessionReason) {
    const auto launch  = hyprspaces::parse_session_reason("launch");
    const auto recover = hyprspaces::parse_session_reason("recover");
    const auto restore = hyprspaces::parse_session_reason("session_restore");

    ASSERT_TRUE(launch.has_value());
    ASSERT_TRUE(recover.has_value());
    ASSERT_TRUE(restore.has_value());
    EXPECT_EQ(*launch, hyprspaces::SessionReason::kLaunch);
    EXPECT_EQ(*recover, hyprspaces::SessionReason::kRecover);
    EXPECT_EQ(*restore, hyprspaces::SessionReason::kSessionRestore);
}

TEST(ConfigParse, RejectsInvalidSessionReason) {
    EXPECT_EQ(hyprspaces::parse_session_reason("nope"), std::nullopt);
}

TEST(ConfigOverrides, AppliesOverrides) {
    const std::vector<hyprspaces::MonitorGroup> base_groups = {
        {.profile = std::nullopt, .name = "left", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0},
    };
    const std::vector<hyprspaces::MonitorGroup> override_groups = {
        {.profile = std::nullopt, .name = "right", .monitors = {"DP-2"}, .workspace_offset = 20},
    };
    const hyprspaces::Config base{
        .monitor_groups        = base_groups,
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
        .session_reason        = hyprspaces::SessionReason::kLaunch,
        .persistent_all        = true,
        .persistent_workspaces = {},
    };
    const hyprspaces::ConfigOverrides overrides{
        .monitor_groups        = override_groups,
        .workspace_count       = 7,
        .wrap_cycling          = false,
        .safe_mode             = false,
        .debug_logging         = true,
        .waybar_enabled        = false,
        .waybar_theme_css      = std::string("/tmp/theme.css"),
        .waybar_class          = std::string("custom-class"),
        .session_enabled       = true,
        .session_autosave      = true,
        .session_autorestore   = true,
        .session_reason        = hyprspaces::SessionReason::kSessionRestore,
        .persistent_all        = false,
        .persistent_workspaces = std::vector<int>{1, 3},
    };

    const auto merged = hyprspaces::apply_overrides(base, overrides);

    ASSERT_EQ(merged.monitor_groups.size(), 1u);
    EXPECT_EQ(merged.monitor_groups[0].name, "right");
    ASSERT_EQ(merged.monitor_groups[0].monitors.size(), 1u);
    EXPECT_EQ(merged.monitor_groups[0].monitors[0], "DP-2");
    EXPECT_EQ(merged.monitor_groups[0].workspace_offset, 20);
    EXPECT_EQ(merged.workspace_count, 7);
    EXPECT_FALSE(merged.wrap_cycling);
    EXPECT_FALSE(merged.safe_mode);
    EXPECT_TRUE(merged.debug_logging);
    EXPECT_FALSE(merged.waybar_enabled);
    ASSERT_TRUE(merged.waybar_theme_css.has_value());
    EXPECT_EQ(*merged.waybar_theme_css, "/tmp/theme.css");
    ASSERT_TRUE(merged.waybar_class.has_value());
    EXPECT_EQ(*merged.waybar_class, "custom-class");
    EXPECT_TRUE(merged.session_enabled);
    EXPECT_TRUE(merged.session_autosave);
    EXPECT_TRUE(merged.session_autorestore);
    EXPECT_EQ(merged.session_reason, hyprspaces::SessionReason::kSessionRestore);
    EXPECT_FALSE(merged.persistent_all);
    ASSERT_EQ(merged.persistent_workspaces.size(), 2u);
    EXPECT_EQ(merged.persistent_workspaces[0], 1);
    EXPECT_EQ(merged.persistent_workspaces[1], 3);
}

TEST(ConfigOverrides, NormalizesOverrideStrings) {
    EXPECT_EQ(hyprspaces::normalize_override_string(""), std::nullopt);
    EXPECT_EQ(hyprspaces::normalize_override_string("   "), std::nullopt);
    EXPECT_EQ(hyprspaces::normalize_override_string("DP-1"), "DP-1");
    EXPECT_EQ(hyprspaces::normalize_override_string("  HDMI-A-1 "), "HDMI-A-1");
}
