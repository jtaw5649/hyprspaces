#include <gtest/gtest.h>

#include "hyprspaces/profiles.hpp"

TEST(ProfileCatalog, StoresProfileDefinitions) {
    hyprspaces::ProfileCatalog catalog;
    catalog.set_active_profile("dev");
    catalog.set_description("dev", "Coding");
    catalog.add_workspace(hyprspaces::ProfileWorkspaceSpec{
        .profile      = "dev",
        .workspace_id = 1,
        .name         = std::string("code"),
        .layout       = std::string("dwindle"),
    });
    catalog.add_window(hyprspaces::ProfileWindowSpec{
        .profile      = "dev",
        .app_id       = std::string("Alacritty"),
        .command      = std::string("alacritty"),
        .workspace_id = 1,
        .floating     = true,
    });

    const auto* profile = catalog.find("dev");
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->description.value_or(""), "Coding");
    ASSERT_EQ(profile->workspaces.size(), 1u);
    ASSERT_EQ(profile->windows.size(), 1u);
}

TEST(ProfileCatalog, RendersProfileLines) {
    hyprspaces::ProfileDefinition profile{
        .name        = "dev",
        .description = std::string("Coding"),
        .workspaces  = {hyprspaces::ProfileWorkspaceSpec{
             .profile      = "dev",
             .workspace_id = 1,
             .name         = std::string("code"),
             .layout       = std::string("dwindle"),
        }},
        .windows     = {hyprspaces::ProfileWindowSpec{
                .profile      = "dev",
                .app_id       = std::string("Alacritty"),
                .command      = std::string("alacritty"),
                .workspace_id = 1,
                .floating     = true,
        }},
    };

    const auto lines = hyprspaces::render_profile_lines(profile);

    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "plugin:hyprspaces:profile = dev");
    EXPECT_EQ(lines[1], "plugin:hyprspaces:profile_description = Coding");
    EXPECT_EQ(lines[2], "plugin:hyprspaces:profile_workspace = 1, name:code, layout:dwindle");
    EXPECT_EQ(lines[3], "plugin:hyprspaces:profile_window = app_id:Alacritty, command:alacritty, workspace:1, floating:true");
}
