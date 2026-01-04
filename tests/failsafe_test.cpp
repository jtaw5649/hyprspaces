#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "hyprspaces/failsafe.hpp"

TEST(FailsafeGuard, ReturnsTrueOnSuccess) {
    bool       handled = false;
    const auto ok      = hyprspaces::failsafe::guard([]() {}, [&](std::string_view, std::string_view) { handled = true; }, "test");

    EXPECT_TRUE(ok);
    EXPECT_FALSE(handled);
}

TEST(FailsafeGuard, ReturnsFalseAndCallsHandlerOnStdException) {
    std::string captured_context;
    std::string captured_message;
    const auto  ok = hyprspaces::failsafe::guard([]() { throw std::runtime_error("boom"); },
                                                [&](std::string_view context, std::string_view message) {
                                                    captured_context = std::string(context);
                                                    captured_message = std::string(message);
                                                },
                                                "test");

    EXPECT_FALSE(ok);
    EXPECT_EQ(captured_context, "test");
    EXPECT_EQ(captured_message, "boom");
}

TEST(FailsafeGuard, ReturnsFalseAndCallsHandlerOnUnknownException) {
    std::string captured_message;
    const auto  ok = hyprspaces::failsafe::guard([]() { throw 1; }, [&](std::string_view, std::string_view message) { captured_message = std::string(message); }, "test");

    EXPECT_FALSE(ok);
    EXPECT_EQ(captured_message, "unknown exception");
}

TEST(FailsafeGuard, SwallowsHandlerExceptions) {
    bool       handler_ran = false;
    const auto ok          = hyprspaces::failsafe::guard([]() { throw std::runtime_error("boom"); },
                                                [&](std::string_view, std::string_view) {
                                                    handler_ran = true;
                                                    throw std::runtime_error("handler failed");
                                                },
                                                "test");

    EXPECT_FALSE(ok);
    EXPECT_TRUE(handler_ran);
}
