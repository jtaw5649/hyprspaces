#include <stdexcept>

#include <gtest/gtest.h>

#include "hyprspaces/monitor_groups.hpp"

TEST(MonitorGroupParse, ParsesGroupWithOffset) {
    const auto group = hyprspaces::parse_monitor_group("name:left-pair, monitors:DP-1;DP-2, workspace_offset:10");

    EXPECT_EQ(group.name, "left-pair");
    ASSERT_EQ(group.monitors.size(), 2u);
    EXPECT_EQ(group.monitors[0], "DP-1");
    EXPECT_EQ(group.monitors[1], "DP-2");
    EXPECT_EQ(group.workspace_offset, 10);
}

TEST(MonitorGroupParse, TrimsWhitespace) {
    const auto group = hyprspaces::parse_monitor_group(" name: left , monitors:  DP-1 ; HDMI-A-1 ");

    EXPECT_EQ(group.name, "left");
    ASSERT_EQ(group.monitors.size(), 2u);
    EXPECT_EQ(group.monitors[0], "DP-1");
    EXPECT_EQ(group.monitors[1], "HDMI-A-1");
}

TEST(MonitorGroupParse, ParsesProfileBinding) {
    const auto group = hyprspaces::parse_monitor_group("profile:home, name:left, monitors:DP-1;DP-2");

    ASSERT_TRUE(group.profile.has_value());
    EXPECT_EQ(*group.profile, "home");
    EXPECT_EQ(group.name, "left");
}

TEST(MonitorGroupParse, RequiresName) {
    EXPECT_THROW(hyprspaces::parse_monitor_group("monitors:DP-1;DP-2"), std::runtime_error);
}

TEST(MonitorGroupParse, RequiresMonitors) {
    EXPECT_THROW(hyprspaces::parse_monitor_group("name:left-pair"), std::runtime_error);
}

TEST(MonitorGroupMapping, FindsGroupForMonitor) {
    const std::vector<hyprspaces::MonitorGroup> groups = {
        {.profile = std::nullopt, .name = "left", .monitors = {"DP-1", "DP-2"}, .workspace_offset = 0},
        {.profile = std::nullopt, .name = "right", .monitors = {"HDMI-A-1"}, .workspace_offset = 20},
    };

    const auto match = hyprspaces::find_group_for_monitor("DP-2", groups);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->group->name, "left");
    EXPECT_EQ(match->monitor_index, 1);
}

TEST(MonitorGroupMapping, ResolvesWorkspaceToGroup) {
    const std::vector<hyprspaces::MonitorGroup> groups = {
        {.profile = std::nullopt, .name = "left", .monitors = {"DP-1", "DP-2"}, .workspace_offset = 0},
        {.profile = std::nullopt, .name = "right", .monitors = {"HDMI-A-1"}, .workspace_offset = 20},
    };

    const auto match = hyprspaces::find_group_for_workspace(7, 5, groups);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->group->name, "left");
    EXPECT_EQ(match->monitor_index, 1);
    EXPECT_EQ(match->normalized_workspace, 2);
}

TEST(MonitorGroupMapping, ComputesWorkspaceIdForMonitor) {
    const hyprspaces::MonitorGroup group{
        .profile          = std::nullopt,
        .name             = "left",
        .monitors         = {"DP-1", "DP-2"},
        .workspace_offset = 0,
    };

    const auto id = hyprspaces::workspace_id_for_group(group, 5, 1, 2);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 7);
}
