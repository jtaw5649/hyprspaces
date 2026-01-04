#include <gtest/gtest.h>

#include "hyprspaces/monitor_groups.hpp"
#include "hyprspaces/monitor_profiles.hpp"
#include "hyprspaces/types.hpp"

TEST(MonitorProfileParse, ParsesProfileWithSelectors) {
    const auto profile = hyprspaces::parse_monitor_profile("name:home, monitors:DP-1;desc:LG UltraWide");

    EXPECT_EQ(profile.name, "home");
    ASSERT_EQ(profile.selectors.size(), 2u);
    EXPECT_EQ(profile.selectors[0].kind, hyprspaces::MonitorSelectorKind::kName);
    EXPECT_EQ(profile.selectors[0].value, "DP-1");
    EXPECT_EQ(profile.selectors[1].kind, hyprspaces::MonitorSelectorKind::kDescription);
    EXPECT_EQ(profile.selectors[1].value, "LG UltraWide");
}

TEST(MonitorProfileSelect, SelectsProfileGroupsWhenMatched) {
    const std::vector<hyprspaces::MonitorProfile> profiles = {
        {.name = "home", .selectors = {{.kind = hyprspaces::MonitorSelectorKind::kName, .value = "DP-1"}, {.kind = hyprspaces::MonitorSelectorKind::kName, .value = "HDMI-A-1"}}},
    };
    const std::vector<hyprspaces::MonitorGroup> groups = {
        {.profile = std::nullopt, .name = "default", .monitors = {"eDP-1"}, .workspace_offset = 0},
        {.profile = std::string("home"), .name = "left-pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 10},
    };
    const std::vector<hyprspaces::MonitorInfo> monitors = {
        {.name = "DP-1", .id = 1, .x = 0, .description = std::string("Dell"), .active_workspace_id = std::nullopt},
        {.name = "HDMI-A-1", .id = 2, .x = 1920, .description = std::string("LG"), .active_workspace_id = std::nullopt},
    };

    const auto selection = hyprspaces::select_monitor_groups(groups, profiles, monitors);

    ASSERT_TRUE(selection.active_profile.has_value());
    EXPECT_EQ(*selection.active_profile, "home");
    ASSERT_EQ(selection.groups.size(), 1u);
    EXPECT_EQ(selection.groups[0].name, "left-pair");
    EXPECT_FALSE(selection.used_fallback);
}

TEST(MonitorProfileSelect, FallsBackToDefaultGroups) {
    const std::vector<hyprspaces::MonitorProfile> profiles = {
        {.name = "home", .selectors = {{.kind = hyprspaces::MonitorSelectorKind::kName, .value = "DP-1"}}},
    };
    const std::vector<hyprspaces::MonitorGroup> groups = {
        {.profile = std::nullopt, .name = "default", .monitors = {"eDP-1"}, .workspace_offset = 0},
        {.profile = std::string("home"), .name = "home-group", .monitors = {"DP-1"}, .workspace_offset = 10},
    };
    const std::vector<hyprspaces::MonitorInfo> monitors = {
        {.name = "eDP-1", .id = 0, .x = 0, .description = std::string("Laptop"), .active_workspace_id = std::nullopt},
    };

    const auto selection = hyprspaces::select_monitor_groups(groups, profiles, monitors);

    EXPECT_FALSE(selection.active_profile.has_value());
    ASSERT_EQ(selection.groups.size(), 1u);
    EXPECT_EQ(selection.groups[0].name, "default");
}

TEST(MonitorProfileSelect, MatchesByDescriptionSelectors) {
    const std::vector<hyprspaces::MonitorProfile> profiles = {
        {.name = "studio", .selectors = {{.kind = hyprspaces::MonitorSelectorKind::kDescription, .value = "LG UltraWide"}}},
    };
    const std::vector<hyprspaces::MonitorGroup> groups = {
        {.profile = std::string("studio"), .name = "studio", .monitors = {"HDMI-A-1"}, .workspace_offset = 0},
    };
    const std::vector<hyprspaces::MonitorInfo> monitors = {
        {.name = "HDMI-A-1", .id = 2, .x = 0, .description = std::string("LG UltraWide"), .active_workspace_id = std::nullopt},
    };

    const auto selection = hyprspaces::select_monitor_groups(groups, profiles, monitors);

    ASSERT_TRUE(selection.active_profile.has_value());
    EXPECT_EQ(*selection.active_profile, "studio");
    ASSERT_EQ(selection.groups.size(), 1u);
    EXPECT_EQ(selection.groups[0].name, "studio");
}
