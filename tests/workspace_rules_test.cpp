#include <gtest/gtest.h>

#include "hyprspaces/workspace_rules.hpp"

TEST(WorkspaceBindings, ResolvesByIdAndName) {
    const std::vector<hyprspaces::WorkspaceInfo> workspaces = {
        {.id = 1, .windows = 0, .name = std::optional<std::string>("1"), .monitor = std::nullopt},
        {.id = 5, .windows = 0, .name = std::optional<std::string>("Web"), .monitor = std::nullopt},
    };
    const std::vector<hyprspaces::WorkspaceRuleBinding> rules = {
        {.workspace_id = 1, .workspace_name = std::nullopt, .monitor = "DP-1"},
        {.workspace_id = std::nullopt, .workspace_name = std::string("name:Web"), .monitor = "HDMI-A-1"},
    };

    const auto bindings = hyprspaces::resolve_workspace_bindings(rules, workspaces);

    ASSERT_EQ(bindings.size(), 2u);
    EXPECT_EQ(bindings.at(1), "DP-1");
    EXPECT_EQ(bindings.at(5), "HDMI-A-1");
}

TEST(WorkspaceBindings, IgnoresUnknownNames) {
    const std::vector<hyprspaces::WorkspaceInfo> workspaces = {
        {.id = 1, .windows = 0, .name = std::optional<std::string>("1"), .monitor = std::nullopt},
    };
    const std::vector<hyprspaces::WorkspaceRuleBinding> rules = {
        {.workspace_id = std::nullopt, .workspace_name = std::string("name:Missing"), .monitor = "DP-1"},
    };

    const auto bindings = hyprspaces::resolve_workspace_bindings(rules, workspaces);

    EXPECT_TRUE(bindings.empty());
}

TEST(WorkspaceBindings, UsesMonitorOverrides) {
    hyprspaces::WorkspaceBindings bindings;
    bindings.emplace(1, "HDMI-A-1");

    const hyprspaces::Config config{
        .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0}},
        .workspace_count       = 2,
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

    EXPECT_EQ(hyprspaces::monitor_for_workspace(1, config, bindings), "HDMI-A-1");
    EXPECT_EQ(hyprspaces::monitor_for_workspace(2, config, bindings), "DP-1");
    EXPECT_EQ(hyprspaces::monitor_for_workspace(4, config, bindings), "HDMI-A-1");
}
