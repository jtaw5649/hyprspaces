#include <gtest/gtest.h>

#include "hyprspaces/pairing.hpp"

TEST(PairingNormalize, IdentityWithinRange) {
    EXPECT_EQ(hyprspaces::normalize_workspace(1, 5), 1);
}

TEST(PairingNormalize, WrapsWithOffset) {
    EXPECT_EQ(hyprspaces::normalize_workspace(12, 10), 2);
}

TEST(PairingNormalize, ReturnsInputWhenOffsetInvalid) {
    EXPECT_EQ(hyprspaces::normalize_workspace(3, 0), 3);
    EXPECT_EQ(hyprspaces::normalize_workspace(3, -2), 3);
}

TEST(PairingCycle, NextWithWrap) {
    EXPECT_EQ(hyprspaces::cycle_target(1, 10, hyprspaces::CycleDirection::kNext, true), 2);
    EXPECT_EQ(hyprspaces::cycle_target(10, 10, hyprspaces::CycleDirection::kNext, true), 1);
}

TEST(PairingCycle, PrevWithWrap) {
    EXPECT_EQ(hyprspaces::cycle_target(1, 10, hyprspaces::CycleDirection::kPrev, true), 10);
    EXPECT_EQ(hyprspaces::cycle_target(2, 10, hyprspaces::CycleDirection::kPrev, true), 1);
}

TEST(PairingCycle, NextWithoutWrap) {
    EXPECT_EQ(hyprspaces::cycle_target(9, 10, hyprspaces::CycleDirection::kNext, false), 10);
    EXPECT_EQ(hyprspaces::cycle_target(10, 10, hyprspaces::CycleDirection::kNext, false), 10);
}

TEST(PairingCycle, PrevWithoutWrap) {
    EXPECT_EQ(hyprspaces::cycle_target(2, 10, hyprspaces::CycleDirection::kPrev, false), 1);
    EXPECT_EQ(hyprspaces::cycle_target(1, 10, hyprspaces::CycleDirection::kPrev, false), 1);
}
