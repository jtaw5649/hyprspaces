#include <gtest/gtest.h>

#include "hyprspaces/registration.hpp"
#include "hyprspaces/version.hpp"

TEST(PluginRegistration, RegistersAllConfigKeys) {
    const auto specs = hyprspaces::config_specs();
    ASSERT_EQ(specs.size(), 14u);

    EXPECT_EQ(specs[0].name, "plugin:hyprspaces:workspace_count");
    EXPECT_EQ(specs[1].name, "plugin:hyprspaces:wrap_cycling");
    EXPECT_EQ(specs[2].name, "plugin:hyprspaces:safe_mode");
    EXPECT_EQ(specs[3].name, "plugin:hyprspaces:debug_logging");
    EXPECT_EQ(specs[4].name, "plugin:hyprspaces:waybar_enabled");
    EXPECT_EQ(specs[5].name, "plugin:hyprspaces:waybar_theme_css");
    EXPECT_EQ(specs[6].name, "plugin:hyprspaces:waybar_class");
    EXPECT_EQ(specs[7].name, "plugin:hyprspaces:notify_session");
    EXPECT_EQ(specs[8].name, "plugin:hyprspaces:notify_timeout_ms");
    EXPECT_EQ(specs[9].name, "plugin:hyprspaces:session_enabled");
    EXPECT_EQ(specs[10].name, "plugin:hyprspaces:session_autosave");
    EXPECT_EQ(specs[11].name, "plugin:hyprspaces:session_autorestore");
    EXPECT_EQ(specs[12].name, "plugin:hyprspaces:session_reason");
    EXPECT_EQ(specs[13].name, "plugin:hyprspaces:persistent_all");
}

TEST(Version, PluginVersionMatchesRelease) {
    EXPECT_EQ(hyprspaces::kPluginVersion, std::string_view("v0.0.1"));
}
