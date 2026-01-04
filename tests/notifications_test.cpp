#include <gtest/gtest.h>

#include "hyprspaces/notifications.hpp"

TEST(SessionNotifications, FormatsSaveSuccess) {
    const auto message = hyprspaces::session_notification_text(hyprspaces::SessionAction::kSave, true, "/tmp/session.json", "");

    EXPECT_EQ(message, "[hyprspaces] Session saved to /tmp/session.json");
}

TEST(SessionNotifications, FormatsRestoreSuccess) {
    const auto message = hyprspaces::session_notification_text(hyprspaces::SessionAction::kRestore, true, "/tmp/session.json", "");

    EXPECT_EQ(message, "[hyprspaces] Session restored from /tmp/session.json");
}

TEST(SessionNotifications, FormatsSaveFailureWithDetail) {
    const auto message = hyprspaces::session_notification_text(hyprspaces::SessionAction::kSave, false, "/tmp/session.json", "missing config");

    EXPECT_EQ(message, "[hyprspaces] Session save failed: missing config");
}

TEST(SessionNotifications, FormatsRestoreFailureWithoutDetail) {
    const auto message = hyprspaces::session_notification_text(hyprspaces::SessionAction::kRestore, false, "/tmp/session.json", "");

    EXPECT_EQ(message, "[hyprspaces] Session restore failed");
}
