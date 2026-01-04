#include <stdexcept>

#include <gtest/gtest.h>

#include "hyprspaces/profiles.hpp"

TEST(ProfileParse, ParsesProfileName) {
    EXPECT_EQ(hyprspaces::parse_profile_name("development"), "development");
}

TEST(ProfileParse, RejectsBlankProfileName) {
    EXPECT_THROW(hyprspaces::parse_profile_name("  "), std::runtime_error);
}

TEST(ProfileParse, ParsesWorkspaceWithActiveProfile) {
    const auto spec = hyprspaces::parse_profile_workspace("1, name:code, layout:dwindle", std::optional<std::string>("dev"));

    EXPECT_EQ(spec.profile, "dev");
    EXPECT_EQ(spec.workspace_id, 1);
    ASSERT_TRUE(spec.name.has_value());
    EXPECT_EQ(*spec.name, "code");
    ASSERT_TRUE(spec.layout.has_value());
    EXPECT_EQ(*spec.layout, "dwindle");
}

TEST(ProfileParse, UsesInlineProfileForWorkspace) {
    const auto spec = hyprspaces::parse_profile_workspace("profile:alt, id:2, name:term", std::nullopt);

    EXPECT_EQ(spec.profile, "alt");
    EXPECT_EQ(spec.workspace_id, 2);
    ASSERT_TRUE(spec.name.has_value());
    EXPECT_EQ(*spec.name, "term");
}

TEST(ProfileParse, ParsesWindowSpec) {
    const auto spec = hyprspaces::parse_profile_window("profile:dev, app_id:Alacritty, command:alacritty -e nvim ., workspace:1, floating:true", std::nullopt);

    EXPECT_EQ(spec.profile, "dev");
    ASSERT_TRUE(spec.app_id.has_value());
    EXPECT_EQ(*spec.app_id, "Alacritty");
    ASSERT_TRUE(spec.command.has_value());
    EXPECT_EQ(*spec.command, "alacritty -e nvim .");
    ASSERT_TRUE(spec.workspace_id.has_value());
    EXPECT_EQ(*spec.workspace_id, 1);
    ASSERT_TRUE(spec.floating.has_value());
    EXPECT_TRUE(*spec.floating);
}

TEST(ProfileParse, RequiresProfileContextForWindow) {
    EXPECT_THROW(hyprspaces::parse_profile_window("app_id:kitty", std::nullopt), std::runtime_error);
}
