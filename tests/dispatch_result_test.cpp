#include <gtest/gtest.h>

#include "hyprspaces/dispatch_result.hpp"

TEST(DispatchResult, ReportsSuccess) {
    const hyprspaces::CommandOutput output{true, "ok"};

    const auto                      result = hyprspaces::dispatch_result_from_output(output);

    EXPECT_FALSE(result.pass_event);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error.empty());
}

TEST(DispatchResult, ReportsFailure) {
    const hyprspaces::CommandOutput output{false, "failed"};

    const auto                      result = hyprspaces::dispatch_result_from_output(output);

    EXPECT_FALSE(result.pass_event);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "failed");
}
