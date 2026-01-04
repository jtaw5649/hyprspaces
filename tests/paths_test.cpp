#include <gtest/gtest.h>

#include "hyprspaces/paths.hpp"

TEST(PathsResolve, PrefersXdgConfigHome) {
    const hyprspaces::EnvConfig env{.home = "/home/tester", .xdg_config_home = "/tmp/xdg", .xdg_state_home = "/tmp/state"};
    const auto                  paths = hyprspaces::resolve_paths(env);

    EXPECT_EQ(paths.base_dir, std::filesystem::path("/tmp/xdg/hyprspaces"));
    EXPECT_EQ(paths.hyprspaces_conf_path, std::filesystem::path("/tmp/xdg/hyprspaces/hyprspaces.conf"));
    EXPECT_EQ(paths.hyprland_conf_path, std::filesystem::path("/tmp/xdg/hypr/hyprland.conf"));
    EXPECT_EQ(paths.profiles_dir, std::filesystem::path("/tmp/xdg/hypr/hyprspaces/profiles"));
    EXPECT_EQ(paths.proc_root, std::filesystem::path("/proc"));
    EXPECT_EQ(paths.state_dir, std::filesystem::path("/tmp/state/hyprspaces"));
    EXPECT_EQ(paths.session_store_path, std::filesystem::path("/tmp/state/hyprspaces/session-store.json"));
}

TEST(PathsResolve, FallsBackToHomeConfig) {
    const hyprspaces::EnvConfig env{.home = "/home/tester", .xdg_config_home = std::nullopt, .xdg_state_home = std::nullopt};
    const auto                  paths = hyprspaces::resolve_paths(env);

    EXPECT_EQ(paths.base_dir, std::filesystem::path("/home/tester/.config/hyprspaces"));
    EXPECT_EQ(paths.hyprspaces_conf_path, std::filesystem::path("/home/tester/.config/hyprspaces/hyprspaces.conf"));
    EXPECT_EQ(paths.hyprland_conf_path, std::filesystem::path("/home/tester/.config/hypr/hyprland.conf"));
    EXPECT_EQ(paths.profiles_dir, std::filesystem::path("/home/tester/.config/hypr/hyprspaces/profiles"));
    EXPECT_EQ(paths.proc_root, std::filesystem::path("/proc"));
    EXPECT_EQ(paths.state_dir, std::filesystem::path("/home/tester/.local/state/hyprspaces"));
    EXPECT_EQ(paths.session_store_path, std::filesystem::path("/home/tester/.local/state/hyprspaces/session-store.json"));
}

TEST(PathsResolve, ReturnsNulloptWithoutHome) {
    const hyprspaces::EnvConfig env{.home = std::nullopt, .xdg_config_home = std::nullopt, .xdg_state_home = std::nullopt};

    const auto                  paths = hyprspaces::try_resolve_paths(env);

    EXPECT_FALSE(paths.has_value());
}
