#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "hyprspaces/actions.hpp"
#include "hyprspaces/config.hpp"
#include "hyprspaces/dispatch.hpp"
#include "hyprspaces/history.hpp"
#include "hyprspaces/hyprctl.hpp"
#include "hyprspaces/logging.hpp"
#include "hyprspaces/workspace_switch.hpp"

namespace {

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

    struct GuardedInvoker : public hyprspaces::HyprctlInvoker {
        struct Call {
            std::string call;
            std::string args;
            std::string format;
        };

        std::vector<Call>        calls;
        std::vector<std::string> responses;
        std::vector<bool>        guard_states;

        std::string              invoke(std::string_view call, std::string_view args, std::string_view format) override {
            guard_states.push_back(hyprspaces::DispatchGuard::active());
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
            .workspace_count       = 10,
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

TEST(Actions, PairedCyclePreservesFocus) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"({"id":12})");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient           client(invoker);

    const hyprspaces::WorkspaceBindings bindings;
    const auto                          result = hyprspaces::paired_cycle(client, test_config(), hyprspaces::CycleDirection::kNext, bindings);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_EQ(invoker.calls[0].call, "activeworkspace");
    EXPECT_EQ(invoker.calls[1].call, "[[BATCH]]");
    EXPECT_EQ(invoker.calls[1].args,
              "dispatch focusmonitor DP-1 ; dispatch workspace 3 ; "
              "dispatch focusmonitor HDMI-A-1 ; dispatch workspace 13");
}

TEST(Actions, PairedSwitchUsesBackAndForthHistory) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"({"id":12})");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient    client(invoker);

    hyprspaces::WorkspaceHistory history;
    history.record("HDMI-A-1", 1, test_config().workspace_count);
    history.record("HDMI-A-1", 2, test_config().workspace_count);
    const hyprspaces::WorkspaceBindings bindings;

    const auto                          result = hyprspaces::paired_switch(client, test_config(), 2, bindings, &history);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_EQ(invoker.calls[1].call, "[[BATCH]]");
    EXPECT_EQ(invoker.calls[1].args,
              "dispatch focusmonitor DP-1 ; dispatch workspace 1 ; "
              "dispatch focusmonitor HDMI-A-1 ; dispatch workspace 11");
}

TEST(Actions, PairedSwitchGuardsDispatch) {
    GuardedInvoker invoker;
    invoker.responses.push_back(R"({"id":12})");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient           client(invoker);

    const hyprspaces::WorkspaceBindings bindings;
    const auto                          result = hyprspaces::paired_switch(client, test_config(), 2, bindings, nullptr);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_FALSE(invoker.guard_states[0]);
    EXPECT_TRUE(invoker.guard_states[1]);
}

TEST(Actions, PairedSwitchUsesExecutorWhenProvided) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"({"id":12})");
    hyprspaces::HyprctlClient           client(invoker);

    const hyprspaces::WorkspaceBindings bindings;
    bool                                called = false;
    hyprspaces::WorkspaceSwitchPlan     captured;
    hyprspaces::WorkspaceSwitchExecutor executor = [&](const hyprspaces::WorkspaceSwitchPlan& plan) -> std::optional<std::string> {
        called   = true;
        captured = plan;
        return std::nullopt;
    };

    const auto result = hyprspaces::paired_switch(client, test_config(), 2, bindings, nullptr, &executor);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(called);
    EXPECT_EQ(captured.requested_workspace, 2);
    EXPECT_EQ(captured.normalized_workspace, 2);
    EXPECT_EQ(captured.group_name, "pair");
    EXPECT_EQ(captured.focus_monitor, "HDMI-A-1");
    ASSERT_EQ(captured.targets.size(), 2u);
    EXPECT_EQ(captured.targets[0].monitor, "DP-1");
    EXPECT_EQ(captured.targets[0].workspace_id, 2);
    EXPECT_EQ(captured.targets[1].monitor, "HDMI-A-1");
    EXPECT_EQ(captured.targets[1].workspace_id, 12);
    ASSERT_EQ(invoker.calls.size(), 1u);
    EXPECT_EQ(invoker.calls[0].call, "activeworkspace");
}

TEST(Actions, PairedSwitchEmitsDebugLogs) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"({"id":12})");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient    client(invoker);

    hyprspaces::WorkspaceHistory history;
    history.record("HDMI-A-1", 1, test_config().workspace_count);
    history.record("HDMI-A-1", 2, test_config().workspace_count);
    const hyprspaces::WorkspaceBindings bindings;

    auto                                config = test_config();
    config.debug_logging                       = true;
    debug_messages.clear();
    hyprspaces::set_debug_log_sink(capture_debug_message);

    const auto result = hyprspaces::paired_switch(client, config, 2, bindings, &history);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::any_of(debug_messages.begin(), debug_messages.end(), [](const auto& entry) { return entry.find("[hyprspaces][debug] paired switch:") != std::string::npos; }));
    EXPECT_TRUE(std::any_of(debug_messages.begin(), debug_messages.end(), [](const auto& entry) {
        return entry.find("[hyprspaces][debug] paired switch targets:") != std::string::npos && entry.find("pair") != std::string::npos;
    }));
    hyprspaces::clear_debug_log_sink();
}

TEST(Actions, PairedMoveWindowDispatchesMoveAndSwitch) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"({"id":12})");
    invoker.responses.push_back("ok");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient           client(invoker);

    const hyprspaces::WorkspaceBindings bindings;
    const auto                          result = hyprspaces::paired_move_window(client, test_config(), 2, bindings);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(invoker.calls.size(), 3u);
    EXPECT_EQ(invoker.calls[1].call, "dispatch");
    EXPECT_EQ(invoker.calls[1].args, "movetoworkspacesilent 12");
    EXPECT_EQ(invoker.calls[2].call, "[[BATCH]]");
}

TEST(Actions, PairedMoveWindowUsesExecutorForSwitch) {
    ScriptedInvoker invoker;
    invoker.responses.push_back(R"({"id":12})");
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient           client(invoker);

    const hyprspaces::WorkspaceBindings bindings;
    bool                                called = false;
    hyprspaces::WorkspaceSwitchPlan     captured;
    hyprspaces::WorkspaceSwitchExecutor executor = [&](const hyprspaces::WorkspaceSwitchPlan& plan) -> std::optional<std::string> {
        called   = true;
        captured = plan;
        return std::nullopt;
    };

    const auto result = hyprspaces::paired_move_window(client, test_config(), 2, bindings, &executor);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(called);
    EXPECT_EQ(captured.group_name, "pair");
    ASSERT_EQ(captured.targets.size(), 2u);
    EXPECT_EQ(captured.targets[0].workspace_id, 2);
    EXPECT_EQ(captured.targets[1].workspace_id, 12);
    ASSERT_EQ(invoker.calls.size(), 2u);
    EXPECT_EQ(invoker.calls[0].call, "activeworkspace");
    EXPECT_EQ(invoker.calls[1].call, "dispatch");
    EXPECT_EQ(invoker.calls[1].args, "movetoworkspacesilent 12");
}
