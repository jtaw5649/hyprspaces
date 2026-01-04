#include <gtest/gtest.h>

#include "hyprspaces/toplevel_state_tracker.hpp"

namespace {

    hyprspaces::SessionProtocolStore::ToplevelState make_state(std::string name, int x) {
        return hyprspaces::SessionProtocolStore::ToplevelState{
            .name       = std::move(name),
            .geometry   = hyprspaces::WindowGeometry{.x = x, .y = 2, .width = 3, .height = 4},
            .floating   = true,
            .pinned     = false,
            .fullscreen = hyprspaces::FullscreenState{.internal = 1, .client = 2},
        };
    }

} // namespace

TEST(ToplevelStateTracker, ReturnsStateOnce) {
    hyprspaces::ToplevelStateTracker tracker;
    tracker.set_states("session", {make_state("editor", 1), make_state("chat", 9)});

    EXPECT_TRUE(tracker.has_pending("session"));
    auto first = tracker.take_state("session", "editor");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->name, "editor");
    ASSERT_TRUE(first->geometry.has_value());
    EXPECT_EQ(first->geometry->x, 1);
    EXPECT_TRUE(first->floating.has_value());
    EXPECT_TRUE(*first->floating);

    auto second = tracker.take_state("session", "chat");
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->name, "chat");
    ASSERT_TRUE(second->geometry.has_value());
    EXPECT_EQ(second->geometry->x, 9);

    EXPECT_FALSE(tracker.has_pending("session"));
    EXPECT_FALSE(tracker.take_state("session", "editor").has_value());
}

TEST(ToplevelStateTracker, IgnoresUnknownSession) {
    hyprspaces::ToplevelStateTracker tracker;
    EXPECT_FALSE(tracker.has_pending("missing"));
    EXPECT_FALSE(tracker.take_state("missing", "window").has_value());
}
