#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "hyprspaces/marks.hpp"
#include "hyprspaces/runtime.hpp"

namespace {

    struct FakeInvoker : public hyprspaces::HyprctlInvoker {
        struct Call {
            std::string call;
            std::string args;
            std::string format;
        };

        std::vector<Call>        calls;
        std::vector<std::string> responses;

        std::string              invoke(std::string_view call, std::string_view args, std::string_view format) override {
            calls.push_back({std::string(call), std::string(args), std::string(format)});
            if (responses.empty()) {
                return {};
            }
            auto response = responses.front();
            responses.erase(responses.begin());
            return response;
        }
    };

    hyprspaces::Paths test_paths(const std::filesystem::path& base_dir) {
        return hyprspaces::Paths{
            .base_dir             = base_dir,
            .hyprspaces_conf_path = base_dir / "hyprspaces.conf",
            .hyprland_conf_path   = base_dir / "hypr" / "hyprland.conf",
            .profiles_dir         = base_dir / "hypr" / "hyprspaces" / "profiles",
            .proc_root            = base_dir / "proc",
            .state_dir            = base_dir / "state" / "hyprspaces",
            .session_store_path   = base_dir / "state" / "hyprspaces" / "session-store.json",
            .waybar_dir           = base_dir / "waybar",
            .waybar_theme_css     = base_dir / "waybar" / "theme.css",
        };
    }

    std::filesystem::path make_unique_temp_dir(std::string_view prefix) {
        const auto         now = std::chrono::steady_clock::now().time_since_epoch().count();
        std::random_device rd;
        return std::filesystem::temp_directory_path() / (std::string(prefix) + "-" + std::to_string(now) + "-" + std::to_string(rd()));
    }

} // namespace

TEST(RuntimeConfig, LoadsConfigWithOverrides) {
    const std::vector<hyprspaces::MonitorGroup> override_groups = {
        {.profile = std::nullopt, .name = "pair", .monitors = {"DP-2", "HDMI-A-2"}, .workspace_offset = 10},
    };
    const hyprspaces::ConfigOverrides overrides{
        .monitor_groups        = override_groups,
        .workspace_count       = 4,
        .wrap_cycling          = false,
        .safe_mode             = false,
        .debug_logging         = true,
        .waybar_enabled        = false,
        .waybar_theme_css      = std::string("/tmp/theme.css"),
        .waybar_class          = std::string("workspace-bars"),
        .session_enabled       = true,
        .session_autosave      = true,
        .session_autorestore   = true,
        .session_reason        = hyprspaces::SessionReason::kRecover,
        .persistent_all        = false,
        .persistent_workspaces = std::vector<int>{1, 3},
    };

    const auto runtime_config = hyprspaces::load_runtime_config(test_paths("/tmp/hyprspaces-runtime"), overrides);

    ASSERT_EQ(runtime_config.config.monitor_groups.size(), 1u);
    EXPECT_EQ(runtime_config.config.monitor_groups[0].name, "pair");
    ASSERT_EQ(runtime_config.config.monitor_groups[0].monitors.size(), 2u);
    EXPECT_EQ(runtime_config.config.monitor_groups[0].monitors[0], "DP-2");
    EXPECT_EQ(runtime_config.config.monitor_groups[0].monitors[1], "HDMI-A-2");
    EXPECT_EQ(runtime_config.config.monitor_groups[0].workspace_offset, 10);
    EXPECT_EQ(runtime_config.config.workspace_count, 4);
    EXPECT_FALSE(runtime_config.config.wrap_cycling);
    EXPECT_FALSE(runtime_config.config.safe_mode);
    EXPECT_TRUE(runtime_config.config.debug_logging);
    EXPECT_FALSE(runtime_config.config.waybar_enabled);
    ASSERT_TRUE(runtime_config.config.waybar_theme_css.has_value());
    EXPECT_EQ(*runtime_config.config.waybar_theme_css, "/tmp/theme.css");
    ASSERT_TRUE(runtime_config.config.waybar_class.has_value());
    EXPECT_EQ(*runtime_config.config.waybar_class, "workspace-bars");
    EXPECT_TRUE(runtime_config.config.session_enabled);
    EXPECT_TRUE(runtime_config.config.session_autosave);
    EXPECT_TRUE(runtime_config.config.session_autorestore);
    EXPECT_EQ(runtime_config.config.session_reason, hyprspaces::SessionReason::kRecover);
    EXPECT_FALSE(runtime_config.config.persistent_all);
    ASSERT_EQ(runtime_config.config.persistent_workspaces.size(), 2u);
    EXPECT_EQ(runtime_config.config.persistent_workspaces[0], 1);
    EXPECT_EQ(runtime_config.config.persistent_workspaces[1], 3);
}

TEST(RuntimeCache, ReloadsWhenOverridesChange) {
    hyprspaces::RuntimeConfigCache cache(test_paths("/tmp/hyprspaces-cache"));

    const auto&                    first       = cache.get(hyprspaces::ConfigOverrides{}, {});
    const int                      first_count = first.config.workspace_count;

    hyprspaces::ConfigOverrides    overrides;
    overrides.workspace_count = 12;
    const auto& second        = cache.get(overrides, {});

    EXPECT_NE(first_count, second.config.workspace_count);
}

TEST(RuntimeStateSnapshot, UsesProviderWhenComplete) {
    FakeInvoker                      invoker;
    hyprspaces::HyprctlClient        client(invoker);

    hyprspaces::RuntimeStateProvider provider;
    provider.active_workspace_id = []() { return std::optional<int>(2); };
    provider.workspaces          = []() {
        return std::optional<std::vector<hyprspaces::WorkspaceInfo>>(
            std::vector<hyprspaces::WorkspaceInfo>{{.id = 2, .windows = 1, .name = std::string("2"), .monitor = std::string("DP-1")}});
    };
    provider.monitors = []() {
        return std::optional<std::vector<hyprspaces::MonitorInfo>>(
            std::vector<hyprspaces::MonitorInfo>{{.name = std::string("DP-1"), .id = 1, .x = 0, .description = std::nullopt, .active_workspace_id = 2}});
    };
    provider.clients = []() {
        return std::optional<std::vector<hyprspaces::ClientInfo>>(std::vector<hyprspaces::ClientInfo>{
            {.address       = "0x1",
             .workspace     = {.id = 2, .name = std::string("2")},
             .class_name    = std::string("kitty"),
             .title         = std::string("term"),
             .initial_class = std::nullopt,
             .initial_title = std::nullopt,
             .app_id        = std::nullopt,
             .pid           = std::nullopt,
             .command       = std::nullopt,
             .geometry      = std::nullopt,
             .floating      = std::nullopt,
             .pinned        = std::nullopt,
             .fullscreen    = std::nullopt,
             .urgent        = std::nullopt},
        });
    };

    const auto snapshot = hyprspaces::resolve_state_snapshot(&provider, client);

    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->active_workspace_id, 2);
    EXPECT_EQ(snapshot->workspaces.size(), 1u);
    EXPECT_EQ(snapshot->monitors.size(), 1u);
    EXPECT_EQ(snapshot->clients.size(), 1u);
    EXPECT_TRUE(invoker.calls.empty());
}

TEST(RuntimeStateSnapshot, FallsBackToHyprctlWhenProviderMissingFields) {
    FakeInvoker invoker;
    invoker.responses.push_back(R"([{"name":"DP-1","id":1,"x":0}])");
    invoker.responses.push_back(R"([{"address":"0x1","workspace":{"id":2,"name":"2"}}])");
    hyprspaces::HyprctlClient        client(invoker);

    hyprspaces::RuntimeStateProvider provider;
    provider.active_workspace_id = []() { return std::optional<int>(2); };
    provider.workspaces          = []() {
        return std::optional<std::vector<hyprspaces::WorkspaceInfo>>(
            std::vector<hyprspaces::WorkspaceInfo>{{.id = 2, .windows = 1, .name = std::string("2"), .monitor = std::string("DP-1")}});
    };
    provider.monitors = []() { return std::optional<std::vector<hyprspaces::MonitorInfo>>(); };
    provider.clients  = []() { return std::optional<std::vector<hyprspaces::ClientInfo>>(); };

    const auto snapshot = hyprspaces::resolve_state_snapshot(&provider, client);

    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->active_workspace_id, 2);
    EXPECT_EQ(snapshot->workspaces.size(), 1u);
    EXPECT_EQ(snapshot->monitors.size(), 1u);
    EXPECT_EQ(snapshot->clients.size(), 1u);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_EQ(invoker.calls[0].call, "monitors");
    EXPECT_EQ(invoker.calls[1].call, "clients");
}

TEST(RuntimeStateSnapshot, ReportsHyprctlFailure) {
    FakeInvoker invoker;
    invoker.responses.push_back("not-json");
    hyprspaces::HyprctlClient        client(invoker);

    hyprspaces::RuntimeStateProvider provider;
    provider.active_workspace_id = []() { return std::optional<int>(2); };
    provider.workspaces          = []() {
        return std::optional<std::vector<hyprspaces::WorkspaceInfo>>(
            std::vector<hyprspaces::WorkspaceInfo>{{.id = 2, .windows = 1, .name = std::string("2"), .monitor = std::string("DP-1")}});
    };
    provider.monitors = []() { return std::optional<std::vector<hyprspaces::MonitorInfo>>(); };
    provider.clients  = []() { return std::optional<std::vector<hyprspaces::ClientInfo>>(); };

    const auto snapshot = hyprspaces::resolve_state_snapshot(&provider, client);

    EXPECT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error(), "state snapshot failed: monitors: invalid json");
    ASSERT_EQ(invoker.calls.size(), 1u);
    EXPECT_EQ(invoker.calls[0].call, "monitors");
}

TEST(RuntimeCommand, RendersStatusJson) {
    FakeInvoker invoker;
    invoker.responses.push_back(R"({"id":7})");
    hyprspaces::HyprctlClient client(invoker);
    const hyprspaces::Config  config{
         .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0}},
         .workspace_count       = 5,
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
    const hyprspaces::RuntimeConfig runtime_config{
        .paths  = test_paths("/tmp/hyprspaces-runtime"),
        .config = config,
    };

    const auto output =
        hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kJson, "status", nullptr, {}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    EXPECT_TRUE(output.success) << output.output;
    const auto json = nlohmann::json::parse(output.output);
    EXPECT_EQ(json.at("status"), "loaded");
    EXPECT_EQ(json.at("active_group").at("name"), "pair");
    EXPECT_EQ(json.at("active_group").at("workspace_id"), 7);
}

TEST(RuntimeCommand, SessionSaveDisabledByDefault) {
    FakeInvoker               invoker;
    hyprspaces::HyprctlClient client(invoker);
    const hyprspaces::Config  config{
         .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0}},
         .workspace_count       = 5,
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
    const hyprspaces::RuntimeConfig runtime_config{
        .paths  = test_paths("/tmp/hyprspaces-runtime"),
        .config = config,
    };

    const auto output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "session save", nullptr, {}, nullptr, nullptr, nullptr, nullptr,
                                                        nullptr, nullptr);

    EXPECT_FALSE(output.success);
    EXPECT_EQ(output.output, "session management disabled");
}

TEST(RuntimeCommand, SessionSaveRequiresProtocolCoordinator) {
    FakeInvoker invoker;
    invoker.responses.push_back("[]");
    hyprspaces::HyprctlClient client(invoker);
    const hyprspaces::Config  config{
         .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0}},
         .workspace_count       = 5,
         .wrap_cycling          = true,
         .safe_mode             = true,
         .debug_logging         = false,
         .waybar_enabled        = true,
         .waybar_theme_css      = std::nullopt,
         .waybar_class          = std::nullopt,
         .session_enabled       = true,
         .session_autosave      = false,
         .session_autorestore   = false,
         .session_reason        = hyprspaces::SessionReason::kSessionRestore,
         .persistent_all        = true,
         .persistent_workspaces = {},
    };
    const hyprspaces::RuntimeConfig runtime_config{
        .paths  = test_paths("/tmp/hyprspaces-runtime"),
        .config = config,
    };

    const auto output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "session save", nullptr, {}, nullptr, nullptr, nullptr, nullptr,
                                                        nullptr, nullptr);

    EXPECT_FALSE(output.success);
    EXPECT_EQ(output.output, "session protocol unavailable");
}

TEST(RuntimeCommand, SessionProfileListUsesCatalog) {
    FakeInvoker                     invoker;
    hyprspaces::HyprctlClient       client(invoker);
    const hyprspaces::RuntimeConfig runtime_config{
        .paths  = test_paths("/tmp/hyprspaces-runtime"),
        .config = hyprspaces::Config{},
    };
    hyprspaces::ProfileCatalog catalog;
    catalog.upsert_profile(hyprspaces::ProfileDefinition{.name = "alpha", .workspaces = {}, .windows = {}});
    catalog.upsert_profile(hyprspaces::ProfileDefinition{.name = "beta", .workspaces = {}, .windows = {}});

    const auto output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "session profile list", nullptr, {}, nullptr, nullptr, nullptr,
                                                        &catalog, nullptr, nullptr);

    EXPECT_TRUE(output.success);
    EXPECT_EQ(output.output, "alpha\nbeta");
}

TEST(RuntimeCommand, WaybarHandlesHyprctlFailure) {
    FakeInvoker invoker;
    invoker.responses.push_back("not-json");
    hyprspaces::HyprctlClient client(invoker);

    const auto                temp_dir = make_unique_temp_dir("hyprspaces-waybar-error");
    std::filesystem::create_directories(temp_dir / "waybar");
    {
        std::ofstream output(temp_dir / "waybar" / "theme.css");
        output << "@define-color foreground #c0caf5;";
    }

    const hyprspaces::Config config{
        .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0}},
        .workspace_count       = 5,
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
    const hyprspaces::RuntimeConfig runtime_config{
        .paths  = test_paths(temp_dir),
        .config = config,
    };

    hyprspaces::RuntimeStateProvider provider;
    provider.active_workspace_id = []() { return std::optional<int>(2); };
    provider.workspaces          = []() {
        return std::optional<std::vector<hyprspaces::WorkspaceInfo>>(
            std::vector<hyprspaces::WorkspaceInfo>{{.id = 2, .windows = 1, .name = std::string("2"), .monitor = std::string("DP-1")}});
    };

    hyprspaces::CommandOutput output{true, ""};
    EXPECT_NO_THROW({
        output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "waybar --enable-waybar", nullptr, {}, nullptr, &provider, nullptr,
                                                 nullptr, nullptr, nullptr);
    });
    EXPECT_FALSE(output.success);
    EXPECT_EQ(output.output, "state snapshot failed: activeworkspace: invalid json");

    std::filesystem::remove_all(temp_dir);
}

TEST(RuntimeCommand, StatusReportsHyprctlFailure) {
    FakeInvoker invoker;
    invoker.responses.push_back("not-json");
    hyprspaces::HyprctlClient client(invoker);

    const hyprspaces::Config  config{
         .monitor_groups        = {{.profile = std::nullopt, .name = "pair", .monitors = {"DP-1", "HDMI-A-1"}, .workspace_offset = 0}},
         .workspace_count       = 5,
         .wrap_cycling          = true,
         .safe_mode             = true,
         .debug_logging         = false,
         .waybar_enabled        = false,
         .waybar_theme_css      = std::nullopt,
         .waybar_class          = std::nullopt,
         .session_enabled       = false,
         .session_autosave      = false,
         .session_autorestore   = false,
         .session_reason        = hyprspaces::SessionReason::kSessionRestore,
         .persistent_all        = true,
         .persistent_workspaces = {},
    };
    const hyprspaces::RuntimeConfig runtime_config{
        .paths  = test_paths("/tmp/hyprspaces-runtime"),
        .config = config,
    };

    hyprspaces::CommandOutput output{true, ""};
    EXPECT_NO_THROW({
        output =
            hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "status", nullptr, {}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    });
    EXPECT_FALSE(output.success);
    EXPECT_EQ(output.output, "activeworkspace: invalid json");
}

TEST(RuntimeCommand, SessionProfileSaveWritesProfileFile) {
    FakeInvoker               invoker;
    hyprspaces::HyprctlClient client(invoker);
    const auto                base_dir = make_unique_temp_dir("hyprspaces-profile-save");
    const auto                paths    = test_paths(base_dir);
    auto                      config   = hyprspaces::Config{};
    config.session_enabled             = true;
    const hyprspaces::RuntimeConfig runtime_config{.paths = paths, .config = config};
    const auto                      cmdline_dir = paths.proc_root / "1234";
    std::filesystem::create_directories(cmdline_dir);
    {
        std::ofstream     cmdline(cmdline_dir / "cmdline", std::ios::binary);
        const std::string payload("alacritty\0-e\0nvim\0.\0", 20);
        cmdline.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }

    hyprspaces::RuntimeStateProvider provider;
    provider.workspaces = []() {
        return std::optional<std::vector<hyprspaces::WorkspaceInfo>>(std::vector<hyprspaces::WorkspaceInfo>{
            {.id = 1, .windows = 2, .name = std::string("code"), .monitor = std::nullopt},
        });
    };
    provider.clients = []() {
        return std::optional<std::vector<hyprspaces::ClientInfo>>(std::vector<hyprspaces::ClientInfo>{
            {.address       = "0x1",
             .workspace     = {.id = 1, .name = std::string("code")},
             .class_name    = std::nullopt,
             .title         = std::string("editor"),
             .initial_class = std::nullopt,
             .initial_title = std::nullopt,
             .app_id        = std::string("alacritty"),
             .pid           = 1234,
             .command       = std::nullopt,
             .geometry      = std::nullopt,
             .floating      = false,
             .pinned        = std::nullopt,
             .fullscreen    = std::nullopt,
             .urgent        = std::nullopt},
        });
    };

    bool                           save_called = false;
    std::string                    save_id;
    hyprspaces::SessionCoordinator coordinator{
        .save =
            [&](std::string_view id) {
                save_called = true;
                save_id.assign(id);
                return std::nullopt;
            },
    };

    hyprspaces::ProfileCatalog catalog;
    const auto output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "session profile save dev", nullptr, {}, nullptr, &provider,
                                                        &coordinator, &catalog, nullptr, nullptr);

    EXPECT_TRUE(output.success);
    EXPECT_EQ(output.output, "ok");
    EXPECT_TRUE(save_called);
    EXPECT_EQ(save_id, "dev");
    const auto filename = paths.profiles_dir / "profile.dev.conf";
    ASSERT_TRUE(std::filesystem::exists(filename));
    std::ifstream     input(filename);
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("plugin:hyprspaces:profile = dev"), std::string::npos);
    EXPECT_NE(contents.find("command:alacritty -e nvim ."), std::string::npos);
    std::filesystem::remove_all(base_dir);
}

TEST(RuntimeCommand, SessionProfileLoadDispatchesProfile) {
    FakeInvoker invoker;
    invoker.responses = {"ok", "ok", "ok", "ok"};
    hyprspaces::HyprctlClient client(invoker);
    auto                      config = hyprspaces::Config{};
    config.session_enabled           = true;
    const hyprspaces::RuntimeConfig runtime_config{.paths = test_paths("/tmp/hyprspaces-runtime"), .config = config};

    hyprspaces::ProfileDefinition   profile{
          .name        = "dev",
          .description = std::nullopt,
          .workspaces  = {hyprspaces::ProfileWorkspaceSpec{.profile = "dev", .workspace_id = 2, .name = std::string("code"), .layout = std::nullopt}},
          .windows     = {hyprspaces::ProfileWindowSpec{.profile       = "dev",
                                                        .app_id        = std::string("alacritty"),
                                                        .initial_class = std::nullopt,
                                                        .title         = std::nullopt,
                                                        .command       = std::string("alacritty"),
                                                        .workspace_id  = 2,
                                                        .floating      = false}},
    };
    hyprspaces::ProfileCatalog catalog;
    catalog.upsert_profile(profile);

    hyprspaces::RuntimeStateProvider provider;
    provider.clients = []() {
        return std::optional<std::vector<hyprspaces::ClientInfo>>(std::vector<hyprspaces::ClientInfo>{
            {.address       = "0xabc",
             .workspace     = {.id = 1, .name = std::string("code")},
             .class_name    = std::nullopt,
             .title         = std::nullopt,
             .initial_class = std::nullopt,
             .initial_title = std::nullopt,
             .app_id        = std::string("alacritty"),
             .pid           = std::nullopt,
             .command       = std::nullopt,
             .geometry      = std::nullopt,
             .floating      = std::nullopt,
             .pinned        = std::nullopt,
             .fullscreen    = std::nullopt,
             .urgent        = std::nullopt},
        });
    };

    bool                           restore_called          = false;
    bool                           restore_before_dispatch = false;
    hyprspaces::SessionCoordinator coordinator{
        .restore =
            [&](std::string_view) {
                restore_called          = true;
                restore_before_dispatch = invoker.calls.empty();
                return std::nullopt;
            },
    };

    const auto output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "session profile load dev", nullptr, {}, nullptr, &provider,
                                                        &coordinator, &catalog, nullptr, nullptr);

    EXPECT_TRUE(output.success);
    EXPECT_EQ(output.output, "ok");
    EXPECT_TRUE(restore_called);
    EXPECT_TRUE(restore_before_dispatch);
    ASSERT_EQ(invoker.calls.size(), 4u);
    EXPECT_EQ(invoker.calls[0].call, "dispatch");
    EXPECT_EQ(invoker.calls[0].args, "workspace 2");
    EXPECT_EQ(invoker.calls[1].call, "dispatch");
    EXPECT_EQ(invoker.calls[1].args, "renameworkspace 2 code");
    EXPECT_EQ(invoker.calls[2].call, "dispatch");
    EXPECT_EQ(invoker.calls[2].args, "movetoworkspace 2,address:0xabc");
    EXPECT_EQ(invoker.calls[3].call, "dispatch");
    EXPECT_EQ(invoker.calls[3].args, "settiled address:0xabc");
}

TEST(RuntimeCommand, MarkSetStoresActiveWindow) {
    FakeInvoker               invoker;
    hyprspaces::HyprctlClient client(invoker);
    auto                      config = hyprspaces::Config{};
    config.safe_mode                 = false;
    const hyprspaces::RuntimeConfig  runtime_config{.paths = test_paths("/tmp/hyprspaces-runtime"), .config = config};

    hyprspaces::RuntimeStateProvider provider;
    provider.active_window_address = []() { return std::optional<std::string>("0xabc"); };
    hyprspaces::MarksStore marks;

    const auto output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "mark set alpha", nullptr, {}, nullptr, &provider, nullptr,
                                                        nullptr, nullptr, &marks);

    EXPECT_TRUE(output.success);
    ASSERT_TRUE(marks.get("alpha").has_value());
    EXPECT_EQ(*marks.get("alpha"), "0xabc");
}

TEST(RuntimeCommand, MarkJumpFocusesMarkedWindow) {
    FakeInvoker invoker;
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient client(invoker);
    auto                      config = hyprspaces::Config{};
    config.safe_mode                 = false;
    const hyprspaces::RuntimeConfig runtime_config{.paths = test_paths("/tmp/hyprspaces-runtime"), .config = config};

    hyprspaces::MarksStore          marks;
    marks.set("alpha", "0xabc");

    const auto output = hyprspaces::run_hyprctl_command(client, runtime_config, hyprspaces::OutputFormat::kNormal, "mark jump alpha", nullptr, {}, nullptr, nullptr, nullptr,
                                                        nullptr, nullptr, &marks);

    EXPECT_TRUE(output.success);
    ASSERT_FALSE(invoker.calls.empty());
    EXPECT_EQ(invoker.calls.back().call, "dispatch");
    EXPECT_EQ(invoker.calls.back().args, "focuswindow address:0xabc");
}
