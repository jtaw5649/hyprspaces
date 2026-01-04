#include <gtest/gtest.h>

#include "hyprspaces/config.hpp"
#include "hyprspaces/protocol_session.hpp"

TEST(SessionRegistry, CreatesNewSessionWhenNoIdProvided) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-1"); });
    std::string                 error;

    const auto                  result = registry.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kSessionRestore, &error);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result->id, "session-1");
    EXPECT_EQ(result->state, hyprspaces::SessionOpenState::kCreated);
    EXPECT_EQ(result->replaced_owner, nullptr);
}

TEST(SessionRegistry, TreatsUnknownIdAsNew) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-new"); });
    std::string                 error;

    const auto                  result = registry.open_session(reinterpret_cast<void*>(0x1), std::string_view("unknown"), hyprspaces::SessionReason::kLaunch, &error);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result->id, "session-new");
    EXPECT_EQ(result->state, hyprspaces::SessionOpenState::kCreated);
}

TEST(SessionRegistry, RejectsSessionReuseBySameClient) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-1"); });
    std::string                 error;

    const auto                  first = registry.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(first.has_value());
    error.clear();

    const auto second = registry.open_session(reinterpret_cast<void*>(0x1), first->id, hyprspaces::SessionReason::kLaunch, &error);

    EXPECT_FALSE(second.has_value());
    EXPECT_EQ(error, "session in use");
}

TEST(SessionRegistry, ReplacesSessionFromOtherClient) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-1"); });
    std::string                 error;

    const auto                  first = registry.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(first.has_value());
    error.clear();

    const auto second = registry.open_session(reinterpret_cast<void*>(0x2), first->id, hyprspaces::SessionReason::kRecover, &error);

    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(second->id, "session-1");
    EXPECT_EQ(second->state, hyprspaces::SessionOpenState::kReplaced);
    EXPECT_EQ(second->replaced_owner, reinterpret_cast<void*>(0x1));
}

TEST(SessionRegistry, RestoresRememberedSessionId) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-new"); });
    std::string                 error;

    EXPECT_TRUE(registry.remember_session("session-1"));

    const auto result = registry.open_session(reinterpret_cast<void*>(0x1), std::string_view("session-1"), hyprspaces::SessionReason::kRecover, &error);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result->id, "session-1");
    EXPECT_EQ(result->state, hyprspaces::SessionOpenState::kRestored);
}

TEST(SessionRegistry, RemovesRememberedSessionId) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-new"); });
    std::string                 error;

    EXPECT_TRUE(registry.remember_session("session-1"));
    EXPECT_TRUE(registry.remove_session("session-1"));

    const auto result = registry.open_session(reinterpret_cast<void*>(0x1), std::string_view("session-1"), hyprspaces::SessionReason::kLaunch, &error);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result->id, "session-new");
    EXPECT_EQ(result->state, hyprspaces::SessionOpenState::kCreated);
}

TEST(SessionRegistry, ReservesToplevelNamesPerSession) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-1"); });
    std::string                 error;

    const auto                  session = registry.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(session.has_value());

    EXPECT_TRUE(registry.reserve_toplevel_name(session->id, "term", &error));
    EXPECT_FALSE(registry.reserve_toplevel_name(session->id, "term", &error));
    EXPECT_EQ(error, "name in use");

    registry.release_toplevel_name(session->id, "term");
    error.clear();
    EXPECT_TRUE(registry.reserve_toplevel_name(session->id, "term", &error));
    EXPECT_TRUE(error.empty());
}

TEST(SessionRegistry, CloseSessionReleasesToplevelNames) {
    hyprspaces::SessionRegistry registry([]() { return std::string("session-1"); });
    std::string                 error;

    const auto                  session = registry.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(session.has_value());

    EXPECT_TRUE(registry.reserve_toplevel_name(session->id, "term", &error));
    registry.close_session(reinterpret_cast<void*>(0x1), session->id);

    error.clear();
    EXPECT_TRUE(registry.reserve_toplevel_name(session->id, "term", &error));
    EXPECT_TRUE(error.empty());
}
