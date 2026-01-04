#include <gtest/gtest.h>

#include "hyprspaces/dispatch.hpp"
#include "hyprspaces/config.hpp"
#include "hyprspaces/pairing.hpp"
#include "hyprspaces/workspace_switch.hpp"

TEST(DispatchBatch, JoinsCommands) {
    const std::vector<hyprspaces::DispatchCommand> commands = {
        {"focusmonitor", "HDMI-A-1"},
        {"workspace", "13"},
    };

    EXPECT_EQ(hyprspaces::dispatch_batch(commands), "dispatch focusmonitor HDMI-A-1 ; dispatch workspace 13");
}

TEST(DispatchBatch, BuildsArgumentFromCommands) {
    hyprspaces::DispatchBatch batch;
    batch.dispatch("movetoworkspacesilent", "2,address:0xabc");

    EXPECT_EQ(batch.to_argument(), "dispatch movetoworkspacesilent 2,address:0xabc");
}

TEST(GroupSwitchSequence, OrdersForPrimaryFocus) {
    const std::vector<hyprspaces::MonitorWorkspaceTarget> targets = {
        {.monitor = "DP-1", .workspace_id = 2},
        {.monitor = "HDMI-A-1", .workspace_id = 12},
    };
    const auto commands = hyprspaces::group_switch_sequence(targets, "DP-1");

    EXPECT_EQ(hyprspaces::dispatch_batch(commands),
              "dispatch focusmonitor HDMI-A-1 ; dispatch workspace 12 ; "
              "dispatch focusmonitor DP-1 ; dispatch workspace 2");
}

TEST(GroupSwitchSequence, OrdersForSecondaryFocus) {
    const std::vector<hyprspaces::MonitorWorkspaceTarget> targets = {
        {.monitor = "DP-1", .workspace_id = 2},
        {.monitor = "HDMI-A-1", .workspace_id = 12},
    };
    const auto commands = hyprspaces::group_switch_sequence(targets, "HDMI-A-1");

    EXPECT_EQ(hyprspaces::dispatch_batch(commands),
              "dispatch focusmonitor DP-1 ; dispatch workspace 2 ; "
              "dispatch focusmonitor HDMI-A-1 ; dispatch workspace 12");
}

TEST(RebalanceSequence, ProducesFullMapping) {
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
    const hyprspaces::WorkspaceBindings bindings;
    const auto                          commands = hyprspaces::rebalance_sequence(config, bindings);
    ASSERT_EQ(commands.size(), 4u);
    EXPECT_EQ(commands[0].dispatcher, "moveworkspacetomonitor");
    EXPECT_EQ(commands[0].argument, "1 DP-1");
    EXPECT_EQ(commands[3].argument, "4 HDMI-A-1");
}

TEST(RebalanceSequence, RespectsBindings) {
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
    hyprspaces::WorkspaceBindings bindings;
    bindings.emplace(1, "HDMI-A-1");

    const auto commands = hyprspaces::rebalance_sequence(config, bindings);

    ASSERT_EQ(commands.size(), 4u);
    EXPECT_EQ(commands[0].argument, "1 HDMI-A-1");
    EXPECT_EQ(commands[1].argument, "2 DP-1");
    EXPECT_EQ(commands[2].argument, "3 HDMI-A-1");
    EXPECT_EQ(commands[3].argument, "4 HDMI-A-1");
}

TEST(MoveWindowTarget, UsesSecondaryWhenFocused) {
    const hyprspaces::Config config{
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
    const auto target = hyprspaces::move_window_target(12, 2, config);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(*target, 12);
}

TEST(MoveWindowTarget, UsesPrimaryWhenFocused) {
    const hyprspaces::Config config{
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
    const auto target = hyprspaces::move_window_target(3, 2, config);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(*target, 2);
}
