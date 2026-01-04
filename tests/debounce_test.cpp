#include <chrono>

#include <gtest/gtest.h>

#include "hyprspaces/debounce.hpp"

using Clock = std::chrono::steady_clock;

TEST(RebalanceDebounce, RunsImmediatelyWhenIdle) {
    hyprspaces::RebalanceDebounce debounce(std::chrono::milliseconds(200));
    const auto                    t0 = Clock::time_point{};

    EXPECT_TRUE(debounce.record_event(t0));
}

TEST(RebalanceDebounce, DefersWhenWithinInterval) {
    hyprspaces::RebalanceDebounce debounce(std::chrono::milliseconds(200));
    const auto                    t0 = Clock::time_point{};

    EXPECT_TRUE(debounce.record_event(t0));
    EXPECT_FALSE(debounce.record_event(t0 + std::chrono::milliseconds(100)));
    EXPECT_FALSE(debounce.flush(t0 + std::chrono::milliseconds(150)));
    EXPECT_TRUE(debounce.flush(t0 + std::chrono::milliseconds(300)));
}

TEST(FocusSwitchDebounce, SuppressesSameWorkspaceWithinInterval) {
    hyprspaces::FocusSwitchDebounce debounce(std::chrono::milliseconds(100));
    const auto                      t0 = Clock::time_point{};

    EXPECT_TRUE(debounce.should_switch(t0, 2));
    EXPECT_FALSE(debounce.should_switch(t0 + std::chrono::milliseconds(50), 2));
}

TEST(FocusSwitchDebounce, AllowsDifferentWorkspaceWithinInterval) {
    hyprspaces::FocusSwitchDebounce debounce(std::chrono::milliseconds(100));
    const auto                      t0 = Clock::time_point{};

    EXPECT_TRUE(debounce.should_switch(t0, 2));
    EXPECT_TRUE(debounce.should_switch(t0 + std::chrono::milliseconds(50), 3));
}

TEST(PendingDebounce, FlushesAfterInterval) {
    hyprspaces::PendingDebounce debounce(std::chrono::milliseconds(100));
    const auto                  t0 = Clock::time_point{};

    debounce.mark(t0);
    EXPECT_FALSE(debounce.flush(t0 + std::chrono::milliseconds(50)));
    EXPECT_TRUE(debounce.flush(t0 + std::chrono::milliseconds(100)));
}

TEST(PendingDebounce, RequiresEventBeforeFlush) {
    hyprspaces::PendingDebounce debounce(std::chrono::milliseconds(100));
    const auto                  t0 = Clock::time_point{};

    EXPECT_FALSE(debounce.flush(t0 + std::chrono::milliseconds(100)));
    debounce.mark(t0);
    EXPECT_TRUE(debounce.flush(t0 + std::chrono::milliseconds(200)));
    EXPECT_FALSE(debounce.flush(t0 + std::chrono::milliseconds(300)));
}

TEST(PendingDebounce, ResetsTimerOnNewEvent) {
    hyprspaces::PendingDebounce debounce(std::chrono::milliseconds(100));
    const auto                  t0 = Clock::time_point{};

    debounce.mark(t0);
    debounce.mark(t0 + std::chrono::milliseconds(50));
    EXPECT_FALSE(debounce.flush(t0 + std::chrono::milliseconds(120)));
    EXPECT_TRUE(debounce.flush(t0 + std::chrono::milliseconds(160)));
}
