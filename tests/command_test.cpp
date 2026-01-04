#include <gtest/gtest.h>

#include "hyprspaces/command.hpp"

TEST(CommandParse, ParsesStatus) {
    const auto result = hyprspaces::parse_command("status");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kStatus);
}

TEST(CommandParse, ParsesPairedSwitch) {
    const auto result = hyprspaces::parse_command("paired switch 3");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kPairedSwitch);
    ASSERT_TRUE(command.workspace_spec.has_value());
    EXPECT_EQ(*command.workspace_spec, "3");
}

TEST(CommandParse, ParsesQuotedWorkspaceSpec) {
    const auto result = hyprspaces::parse_command("paired switch \"name:Better anime\"");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kPairedSwitch);
    ASSERT_TRUE(command.workspace_spec.has_value());
    EXPECT_EQ(*command.workspace_spec, "name:Better anime");
}

TEST(CommandParse, RejectsExtraWorkspaceTokens) {
    const auto result = hyprspaces::parse_command("paired switch name:Better anime");
    EXPECT_TRUE(std::holds_alternative<hyprspaces::ParseError>(result));
}

TEST(CommandParse, RejectsUnterminatedQuote) {
    const auto result = hyprspaces::parse_command("paired switch \"name:Better anime");
    EXPECT_TRUE(std::holds_alternative<hyprspaces::ParseError>(result));
}

TEST(CommandParse, RejectsMissingArgument) {
    const auto result = hyprspaces::parse_command("paired switch");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::ParseError>(result));
}

TEST(CommandParse, ParsesSetupInstallWithWaybarFlag) {
    const auto result = hyprspaces::parse_command("setup install --waybar");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kSetupInstall);
    EXPECT_TRUE(command.setup_waybar);
}

TEST(CommandParse, ParsesSessionSave) {
    const auto result = hyprspaces::parse_command("session save");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kSessionSave);
}

TEST(CommandParse, ParsesSessionRestore) {
    const auto result = hyprspaces::parse_command("session restore");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kSessionRestore);
}

TEST(CommandParse, ParsesSessionProfileSave) {
    const auto result = hyprspaces::parse_command("session profile save dev");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kSessionProfileSave);
    ASSERT_TRUE(command.profile_name.has_value());
    EXPECT_EQ(*command.profile_name, "dev");
}

TEST(CommandParse, ParsesSessionProfileLoad) {
    const auto result = hyprspaces::parse_command("session profile load dev");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kSessionProfileLoad);
    ASSERT_TRUE(command.profile_name.has_value());
    EXPECT_EQ(*command.profile_name, "dev");
}

TEST(CommandParse, ParsesSessionProfileList) {
    const auto result = hyprspaces::parse_command("session profile list");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kSessionProfileList);
}

TEST(CommandParse, RejectsSessionProfileMissingName) {
    const auto result = hyprspaces::parse_command("session profile save");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::ParseError>(result));
}

TEST(CommandParse, ParsesMarkSet) {
    const auto result = hyprspaces::parse_command("mark set alpha");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kMarkSet);
    ASSERT_TRUE(command.mark_name.has_value());
    EXPECT_EQ(*command.mark_name, "alpha");
}

TEST(CommandParse, ParsesMarkJump) {
    const auto result = hyprspaces::parse_command("mark jump beta");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kMarkJump);
    ASSERT_TRUE(command.mark_name.has_value());
    EXPECT_EQ(*command.mark_name, "beta");
}

TEST(CommandParse, RejectsMarkMissingName) {
    const auto result = hyprspaces::parse_command("mark set");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::ParseError>(result));
}

TEST(CommandParse, RequiresWaybarEnableFlag) {
    const auto result = hyprspaces::parse_command("waybar");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::ParseError>(result));
}

TEST(CommandParse, ParsesWaybarThemeOverride) {
    const auto result = hyprspaces::parse_command("waybar --enable-waybar --theme-css /tmp/theme.css");
    ASSERT_TRUE(std::holds_alternative<hyprspaces::Command>(result));
    const auto command = std::get<hyprspaces::Command>(result);
    EXPECT_EQ(command.kind, hyprspaces::CommandKind::kWaybar);
    EXPECT_TRUE(command.waybar_enabled);
    ASSERT_TRUE(command.waybar_theme_css.has_value());
    EXPECT_EQ(*command.waybar_theme_css, "/tmp/theme.css");
}
