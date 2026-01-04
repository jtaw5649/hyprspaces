#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

#include "hyprspaces/config.hpp"
#include "hyprspaces/paths.hpp"
#include "hyprspaces/setup.hpp"
#include "hyprspaces/types.hpp"

namespace {

    std::filesystem::path make_temp_dir() {
        const auto base   = std::filesystem::temp_directory_path();
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto       dir    = base / ("hyprspaces_setup_" + unique);
        std::filesystem::create_directories(dir);
        return dir;
    }

    std::string read_fixture(const std::string& name) {
        const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / "fixtures" / name;
        std::ifstream               input(path);
        std::ostringstream          buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    std::string read_file(const std::filesystem::path& path) {
        std::ifstream      input(path);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

} // namespace

TEST(SetupConfig, RendersHyprspacesConf) {
    EXPECT_EQ(hyprspaces::render_hyprspaces_conf(), read_fixture("hyprspaces.conf"));
}

TEST(SetupConfig, EnsuresHyprspacesConfWhenMissing) {
    const auto dir  = make_temp_dir();
    const auto path = dir / "hyprspaces.conf";

    const auto created = hyprspaces::ensure_hyprspaces_conf(path);

    EXPECT_TRUE(created);
    EXPECT_EQ(read_file(path), read_fixture("hyprspaces.conf"));
    std::filesystem::remove_all(dir);
}

TEST(SetupConfig, EnsuresHyprlandConfSourceWhenMissing) {
    const auto dir          = make_temp_dir();
    const auto path         = dir / "hypr" / "hyprland.conf";
    const auto include_path = dir / "hyprspaces" / "hyprspaces.conf";

    const auto created = hyprspaces::ensure_hyprland_conf_source(path, include_path);

    EXPECT_TRUE(created);
    EXPECT_EQ(read_file(path), "source = " + include_path.string() + "\n");
    std::filesystem::remove_all(dir);
}

TEST(SetupConfig, ReportsHyprspacesConfWriteFailure) {
    const auto dir  = make_temp_dir();
    const auto path = dir / "hyprspaces.conf";

    std::error_code ec;
    std::filesystem::permissions(dir,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace,
                                 ec);
    if (ec) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hyprspaces::ensure_hyprspaces_conf(path));
    EXPECT_FALSE(std::filesystem::exists(path));
    std::filesystem::remove_all(dir);
}

TEST(SetupConfig, ReportsHyprlandConfSourceWriteFailure) {
    const std::filesystem::path full_path("/dev/full");
    if (!std::filesystem::exists(full_path)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hyprspaces::ensure_hyprland_conf_source(full_path, "/tmp/hyprspaces.conf"));
}

TEST(SetupConfig, RejectsNonRegularHyprlandConfSource) {
    const std::filesystem::path null_path("/dev/null");
    if (!std::filesystem::exists(null_path)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hyprspaces::ensure_hyprland_conf_source(null_path, "/tmp/hyprspaces.conf"));
}

TEST(SetupConfig, AppendsHyprlandConfSourceWhenMissing) {
    const auto dir          = make_temp_dir();
    const auto path         = dir / "hypr" / "hyprland.conf";
    const auto include_path = dir / "hyprspaces" / "hyprspaces.conf";
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream output(path);
        output << "bind = SUPER, Q, exec, kitty";
    }

    const auto created = hyprspaces::ensure_hyprland_conf_source(path, include_path);

    EXPECT_TRUE(created);
    EXPECT_EQ(read_file(path), "bind = SUPER, Q, exec, kitty\nsource = " + include_path.string() + "\n");
    std::filesystem::remove_all(dir);
}

TEST(SetupConfig, SkipsHyprlandConfSourceWhenPresent) {
    const auto dir          = make_temp_dir();
    const auto path         = dir / "hypr" / "hyprland.conf";
    const auto include_path = dir / "hyprspaces" / "hyprspaces.conf";
    const auto contents     = "source = " + include_path.string() + "\n";
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream output(path);
        output << contents;
    }

    const auto created = hyprspaces::ensure_hyprland_conf_source(path, include_path);

    EXPECT_FALSE(created);
    EXPECT_EQ(read_file(path), contents);
    std::filesystem::remove_all(dir);
}

TEST(SetupConfig, BootstrapsHyprspacesConfigAndSource) {
    const auto              dir = make_temp_dir();
    const hyprspaces::Paths paths{
        .base_dir             = dir / "hyprspaces",
        .hyprspaces_conf_path = dir / "hyprspaces" / "hyprspaces.conf",
        .hyprland_conf_path   = dir / "hypr" / "hyprland.conf",
        .profiles_dir         = dir / "hypr" / "hyprspaces" / "profiles",
        .proc_root            = "/proc",
        .state_dir            = dir / "state" / "hyprspaces",
        .session_store_path   = dir / "state" / "hyprspaces" / "session-store.json",
        .waybar_dir           = dir / "hyprspaces" / "waybar",
        .waybar_theme_css     = dir / "hyprspaces" / "waybar" / "theme.css",
    };

    const auto changed = hyprspaces::ensure_hyprspaces_bootstrap(paths);

    EXPECT_TRUE(changed);
    EXPECT_EQ(read_file(paths.hyprspaces_conf_path), read_fixture("hyprspaces.conf"));
    EXPECT_EQ(read_file(paths.hyprland_conf_path), "source = " + paths.hyprspaces_conf_path.string() + "\n");
    EXPECT_TRUE(std::filesystem::exists(paths.profiles_dir));
    std::filesystem::remove_all(dir);
}

TEST(SetupRules, BuildsWorkspaceRules) {
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

    const auto rules = hyprspaces::workspace_rules_for_config(config);

    ASSERT_EQ(rules.size(), 4u);
    EXPECT_TRUE(rules[0].is_default);
    EXPECT_EQ(rules[0].monitor, "DP-1");
    EXPECT_EQ(rules[2].monitor, "HDMI-A-1");
    EXPECT_TRUE(rules[2].is_default);
}

TEST(SetupRules, RendersWorkspaceRuleLines) {
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

    const auto lines = hyprspaces::workspace_rule_lines_for_config(config);

    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0],
              "1, monitor:DP-1, default:true, persistent:true, "
              "layoutopt:hyprspaces:1");
    EXPECT_EQ(lines[1], "2, monitor:DP-1, persistent:true, layoutopt:hyprspaces:1");
    EXPECT_EQ(lines[2],
              "3, monitor:HDMI-A-1, default:true, persistent:true, "
              "layoutopt:hyprspaces:1");
    EXPECT_EQ(lines[3], "4, monitor:HDMI-A-1, persistent:true, layoutopt:hyprspaces:1");
}

TEST(SetupRules, RespectsPersistentWorkspaces) {
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
        .persistent_all        = false,
        .persistent_workspaces = {1, 3},
    };

    const auto lines = hyprspaces::workspace_rule_lines_for_config(config);

    ASSERT_EQ(lines.size(), 4u);
    EXPECT_NE(lines[0].find("persistent:true"), std::string::npos);
    EXPECT_EQ(lines[1].find("persistent:true"), std::string::npos);
    EXPECT_NE(lines[2].find("persistent:true"), std::string::npos);
    EXPECT_EQ(lines[3].find("persistent:true"), std::string::npos);
}

TEST(SetupWaybarAssets, InstallsAndUninstallsAssets) {
    const auto              dir = make_temp_dir();
    const hyprspaces::Paths paths{
        .base_dir             = dir,
        .hyprspaces_conf_path = dir / "hyprspaces.conf",
        .hyprland_conf_path   = dir / "hyprland.conf",
        .profiles_dir         = dir / "hypr" / "hyprspaces" / "profiles",
        .proc_root            = "/proc",
        .state_dir            = dir / "state" / "hyprspaces",
        .session_store_path   = dir / "state" / "hyprspaces" / "session-store.json",
        .waybar_dir           = dir / "waybar",
        .waybar_theme_css     = dir / "waybar" / "theme.css",
    };

    const auto install_error = hyprspaces::install_waybar_assets(paths, std::nullopt);
    ASSERT_FALSE(install_error.has_value());

    EXPECT_EQ(read_file(paths.waybar_dir / "workspaces.json"), read_fixture("workspaces.json"));
    EXPECT_EQ(read_file(paths.waybar_dir / "workspaces.css"), read_fixture("workspaces.css"));

    const auto uninstall_error = hyprspaces::uninstall_waybar_assets(paths);
    ASSERT_FALSE(uninstall_error.has_value());
    EXPECT_FALSE(std::filesystem::exists(paths.waybar_dir));
    std::filesystem::remove_all(dir);
}

TEST(SetupWaybarAssets, ReportsThemeWriteFailure) {
    const std::filesystem::path full_path("/dev/full");
    if (!std::filesystem::exists(full_path)) {
        GTEST_SKIP();
    }

    const auto              dir = make_temp_dir();
    const hyprspaces::Paths paths{
        .base_dir             = dir,
        .hyprspaces_conf_path = dir / "hyprspaces.conf",
        .hyprland_conf_path   = dir / "hyprland.conf",
        .profiles_dir         = dir / "hypr" / "hyprspaces" / "profiles",
        .proc_root            = "/proc",
        .state_dir            = dir / "state" / "hyprspaces",
        .session_store_path   = dir / "state" / "hyprspaces" / "session-store.json",
        .waybar_dir           = dir / "waybar",
        .waybar_theme_css     = full_path,
    };

    const auto error = hyprspaces::install_waybar_assets(paths, std::nullopt);

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error, "unable to write " + full_path.string());
    std::filesystem::remove_all(dir);
}
