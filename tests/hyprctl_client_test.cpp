#include <gtest/gtest.h>

#include "hyprspaces/hyprctl.hpp"

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

} // namespace

TEST(HyprctlClient, RequestsActiveWorkspaceJson) {
    FakeInvoker invoker;
    invoker.responses.push_back(R"({"id":7})");
    hyprspaces::HyprctlClient client(invoker);

    const auto                result = client.active_workspace_id();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 7);

    ASSERT_EQ(invoker.calls.size(), 1u);
    EXPECT_EQ(invoker.calls[0].call, "activeworkspace");
    EXPECT_EQ(invoker.calls[0].format, "j");
}

TEST(HyprctlClient, ActiveWorkspaceReportsInvalidJson) {
    FakeInvoker invoker;
    invoker.responses.push_back("not-json");
    hyprspaces::HyprctlClient client(invoker);

    const auto                result = client.active_workspace_id();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().context, "activeworkspace");
    EXPECT_EQ(result.error().message, "invalid json");
}

TEST(HyprctlClient, DispatchesBatch) {
    FakeInvoker invoker;
    invoker.responses.push_back("ok");
    hyprspaces::HyprctlClient                      client(invoker);
    const std::vector<hyprspaces::DispatchCommand> commands = {
        {"focusmonitor", "DP-1"},
        {"workspace", "2"},
    };

    const auto output = client.dispatch_batch(commands);

    EXPECT_EQ(output, "ok");
    ASSERT_EQ(invoker.calls.size(), 1u);
    EXPECT_EQ(invoker.calls[0].call, "[[BATCH]]");
    EXPECT_EQ(invoker.calls[0].args, "dispatch focusmonitor DP-1 ; dispatch workspace 2");
}
