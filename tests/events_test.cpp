#include <algorithm>
#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "hyprspaces/config.hpp"
#include "hyprspaces/debounce.hpp"
#include "hyprspaces/dispatch.hpp"
#include "hyprspaces/events.hpp"
#include "hyprspaces/history.hpp"
#include "hyprspaces/hyprctl.hpp"
#include "hyprspaces/logging.hpp"
#include "hyprspaces/workspace_switch.hpp"
#include "hyprspaces/workspace_rules.hpp"

namespace {

    using Clock = std::chrono::steady_clock;

    struct ScriptedInvoker : public hyprspaces::HyprctlInvoker {
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

    hyprspaces::Config test_config() {
        return hyprspaces::Config{
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
    }

    std::vector<std::string> debug_messages;

    void                     capture_debug_message(std::string_view message) {
        debug_messages.emplace_back(message);
    }

} // namespace

TEST(FocusSwitch, UsesMonitorNameFromEvent) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"([{"id":2,"windows":1,"name":"2","monitor":"DP-1"},{"id":4,"windows":1,"name":"4","monitor":"HDMI-A-1"}])");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = 3,
        .window_address = std::nullopt,
        .monitor_name   = std::string("HDMI-A-1"),
    };

    const auto did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce);

    EXPECT_TRUE(did_switch);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_EQ(invoker.calls[0].call, "workspaces");
    EXPECT_EQ(invoker.calls[1].call, "[[BATCH]]");
    EXPECT_EQ(invoker.calls[1].args,
              "dispatch focusmonitor DP-1 ; dispatch workspace 1 ; "
              "dispatch focusmonitor HDMI-A-1 ; dispatch workspace 3");
}

TEST(FocusSwitch, UsesExecutorWhenProvided) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"([{"id":2,"windows":1,"name":"2","monitor":"DP-1"},{"id":4,"windows":1,"name":"4","monitor":"HDMI-A-1"}])");
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = 3,
        .window_address = std::nullopt,
        .monitor_name   = std::string("HDMI-A-1"),
    };
    bool                                called = false;
    hyprspaces::WorkspaceSwitchPlan     captured;
    hyprspaces::WorkspaceSwitchExecutor executor = [&](const hyprspaces::WorkspaceSwitchPlan& plan) -> std::optional<std::string> {
        called   = true;
        captured = plan;
        return std::nullopt;
    };

    const auto did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce, {}, {}, nullptr, &executor);

    EXPECT_TRUE(did_switch);
    EXPECT_TRUE(called);
    EXPECT_EQ(captured.requested_workspace, 3);
    EXPECT_EQ(captured.normalized_workspace, 1);
    EXPECT_EQ(captured.group_name, "pair");
    EXPECT_EQ(captured.focus_monitor, "HDMI-A-1");
    ASSERT_EQ(captured.targets.size(), 2u);
    EXPECT_EQ(captured.targets[0].monitor, "DP-1");
    EXPECT_EQ(captured.targets[0].workspace_id, 1);
    EXPECT_EQ(captured.targets[1].monitor, "HDMI-A-1");
    EXPECT_EQ(captured.targets[1].workspace_id, 3);
    ASSERT_EQ(invoker.calls.size(), 1u);
    EXPECT_EQ(invoker.calls[0].call, "workspaces");
}

TEST(FocusSwitch, SkipsWhenDispatchGuardActive) {
    ScriptedInvoker              invoker;
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = 3,
        .window_address = std::nullopt,
        .monitor_name   = std::string("HDMI-A-1"),
    };

    hyprspaces::DispatchGuard guard;
    const auto                did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce);

    EXPECT_FALSE(did_switch);
    EXPECT_TRUE(invoker.calls.empty());
}

TEST(FocusSwitch, EmitsDebugLogs) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"([{"id":2,"windows":1,"name":"2","monitor":"DP-1"},{"id":4,"windows":1,"name":"4","monitor":"HDMI-A-1"}])");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = 3,
        .window_address = std::nullopt,
        .monitor_name   = std::string("HDMI-A-1"),
    };

    auto config          = test_config();
    config.debug_logging = true;
    debug_messages.clear();
    hyprspaces::set_debug_log_sink(capture_debug_message);

    const auto did_switch = hyprspaces::focus_switch_for_event(client, config, event, debounce);

    EXPECT_TRUE(did_switch);
    EXPECT_TRUE(std::any_of(debug_messages.begin(), debug_messages.end(), [](const auto& entry) { return entry.find("[hyprspaces][debug] focus event:") != std::string::npos; }));
    EXPECT_TRUE(std::any_of(debug_messages.begin(), debug_messages.end(), [](const auto& entry) {
        return entry.find("[hyprspaces][debug] focus switch targets:") != std::string::npos && entry.find("pair") != std::string::npos;
    }));
    hyprspaces::clear_debug_log_sink();
}

TEST(FocusSwitch, AppliesBindingsForWorkspacePair) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"([{"id":2,"windows":1,"name":"2","monitor":"DP-1"},{"id":4,"windows":1,"name":"4","monitor":"HDMI-A-1"}])");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = 3,
        .window_address = std::nullopt,
        .monitor_name   = std::string("HDMI-A-1"),
    };
    hyprspaces::WorkspaceBindings bindings;
    bindings[1] = "HDMI-A-1";
    bindings[3] = "DP-1";

    const auto did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce, {}, bindings);

    EXPECT_TRUE(did_switch);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_EQ(invoker.calls[0].call, "workspaces");
    EXPECT_EQ(invoker.calls[1].call, "[[BATCH]]");
    EXPECT_EQ(invoker.calls[1].args,
              "dispatch focusmonitor DP-1 ; dispatch workspace 3 ; "
              "dispatch focusmonitor HDMI-A-1 ; dispatch workspace 1");
}

TEST(FocusSwitch, RecordsHistoryForMonitor) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"([{"id":2,"windows":1,"name":"2","monitor":"DP-1"},{"id":4,"windows":1,"name":"4","monitor":"HDMI-A-1"}])");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = 3,
        .window_address = std::nullopt,
        .monitor_name   = std::string("HDMI-A-1"),
    };
    hyprspaces::WorkspaceHistory history;

    const auto                   did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce, {}, {}, &history);

    EXPECT_TRUE(did_switch);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_EQ(invoker.calls[0].call, "workspaces");
    EXPECT_EQ(history.current("HDMI-A-1"), 1);
    EXPECT_FALSE(history.previous("HDMI-A-1").has_value());
}

TEST(FocusSwitch, SkipsWindowFocusEvents) {
    ScriptedInvoker              invoker;
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = std::nullopt,
        .window_address = std::string("0x123"),
        .monitor_name   = std::nullopt,
    };

    const auto did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce);

    EXPECT_FALSE(did_switch);
    EXPECT_TRUE(invoker.calls.empty());
}

TEST(FocusSwitch, SkipsWindowFocusWithResolvers) {
    ScriptedInvoker            invoker;
    hyprspaces::HyprctlClient  client(invoker);
    auto                       debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                 t0       = Clock::time_point{};
    hyprspaces::FocusResolvers resolvers;
    resolvers.workspace_for_window  = [](std::string_view) { return 3; };
    resolvers.monitor_for_workspace = [](int) { return std::optional<std::string>("HDMI-A-1"); };
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = std::nullopt,
        .window_address = std::string("0x123"),
        .monitor_name   = std::nullopt,
    };

    const auto did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce, resolvers);

    EXPECT_FALSE(did_switch);
    EXPECT_TRUE(invoker.calls.empty());
}

TEST(FocusSwitch, DebouncesSameWorkspace) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"([{"id":1,"windows":1,"name":"1","monitor":"DP-1"},{"id":4,"windows":1,"name":"4","monitor":"HDMI-A-1"}])");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent first{
        .at             = t0,
        .workspace_id   = 2,
        .window_address = std::nullopt,
        .monitor_name   = std::string("DP-1"),
    };
    const hyprspaces::FocusEvent second{
        .at             = t0 + std::chrono::milliseconds(50),
        .workspace_id   = 2,
        .window_address = std::nullopt,
        .monitor_name   = std::string("DP-1"),
    };

    EXPECT_TRUE(hyprspaces::focus_switch_for_event(client, test_config(), first, debounce));
    EXPECT_FALSE(hyprspaces::focus_switch_for_event(client, test_config(), second, debounce));
    EXPECT_EQ(invoker.calls.size(), 2u);
}

TEST(FocusSwitch, SkipsWhenWorkspacesAlreadyCorrect) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"([{"id":1,"windows":1,"name":"1","monitor":"DP-1"},{"id":3,"windows":1,"name":"3","monitor":"HDMI-A-1"}])");
    hyprspaces::HyprctlClient    client(invoker);
    auto                         debounce = hyprspaces::FocusSwitchDebounce(std::chrono::milliseconds(100));
    const auto                   t0       = Clock::time_point{};
    const hyprspaces::FocusEvent event{
        .at             = t0,
        .workspace_id   = 1,
        .window_address = std::nullopt,
        .monitor_name   = std::string("DP-1"),
    };

    const auto did_switch = hyprspaces::focus_switch_for_event(client, test_config(), event, debounce);

    EXPECT_FALSE(did_switch);
    ASSERT_EQ(invoker.calls.size(), 1u);
    EXPECT_EQ(invoker.calls[0].call, "workspaces");
}

TEST(Rebalance, DispatchesBatchOnMonitorEvent) {
    ScriptedInvoker invoker;
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient     client(invoker);
    auto                          debounce = hyprspaces::RebalanceDebounce(std::chrono::milliseconds(200));
    const auto                    t0       = Clock::time_point{};
    hyprspaces::WorkspaceBindings bindings;
    bindings[1] = "HDMI-A-1";

    const auto did_rebalance = hyprspaces::rebalance_for_event(client, test_config(), hyprspaces::MonitorEventKind::kAdded, debounce, t0, bindings);

    EXPECT_TRUE(did_rebalance);
    ASSERT_EQ(invoker.calls.size(), 1u);
    EXPECT_EQ(invoker.calls[0].call, "[[BATCH]]");
    EXPECT_EQ(invoker.calls[0].args,
              "dispatch moveworkspacetomonitor 1 HDMI-A-1 ; "
              "dispatch moveworkspacetomonitor 2 DP-1 ; "
              "dispatch moveworkspacetomonitor 3 HDMI-A-1 ; "
              "dispatch moveworkspacetomonitor 4 HDMI-A-1");
}
