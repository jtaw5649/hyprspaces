#include <gtest/gtest.h>

#include "hyprspaces/history.hpp"

TEST(WorkspaceHistory, RecordsPerMonitor) {
    hyprspaces::WorkspaceHistory history;

    history.record("DP-1", 3, 2);
    EXPECT_EQ(history.current("DP-1"), 1);
    EXPECT_FALSE(history.previous("DP-1").has_value());

    history.record("DP-1", 4, 2);
    EXPECT_EQ(history.current("DP-1"), 2);
    EXPECT_EQ(history.previous("DP-1"), 1);

    history.record("HDMI-A-1", 1, 2);
    EXPECT_EQ(history.current("HDMI-A-1"), 1);
    EXPECT_FALSE(history.previous("HDMI-A-1").has_value());
}

TEST(WorkspaceHistory, IgnoresRepeatedWorkspace) {
    hyprspaces::WorkspaceHistory history;

    history.record("DP-1", 1, 3);
    history.record("DP-1", 2, 3);
    history.record("DP-1", 5, 3);

    EXPECT_EQ(history.current("DP-1"), 2);
    EXPECT_EQ(history.previous("DP-1"), 1);
}

TEST(WorkspaceHistory, IgnoresInvalidInputs) {
    hyprspaces::WorkspaceHistory history;

    history.record("", 1, 3);
    history.record("DP-1", 0, 3);
    history.record("DP-1", 1, 0);

    EXPECT_FALSE(history.current("DP-1").has_value());
}
