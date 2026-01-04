#include <source_location>
#include <string>

#include <gtest/gtest.h>

#include "hyprspaces/logging.hpp"

TEST(LoggingFormat, PrefixesContext) {
    const auto text = hyprspaces::format_log_entry("focus sync", "failed to open config file");

    EXPECT_EQ(text, "[hyprspaces] focus sync: failed to open config file");
}

TEST(LoggingFormat, HandlesEmptyContext) {
    const auto text = hyprspaces::format_log_entry("", "loaded");

    EXPECT_EQ(text, "[hyprspaces] loaded");
}

namespace {

    std::string g_debug_message;
    std::string g_error_message;

    void        capture_debug_message(std::string_view message) {
        g_debug_message = std::string(message);
    }

    void capture_error_message(std::string_view message) {
        g_error_message = std::string(message);
    }

}

TEST(LoggingFormat, DebugPrefixesContext) {
    const auto text = hyprspaces::format_debug_entry("paired switch", "workspace=2");

    EXPECT_EQ(text, "[hyprspaces][debug] paired switch: workspace=2");
}

TEST(LoggingFormat, DebugHandlesEmptyContext) {
    const auto text = hyprspaces::format_debug_entry("", "ready");

    EXPECT_EQ(text, "[hyprspaces][debug] ready");
}

TEST(LoggingFormat, DebugLogUsesSinkWhenEnabled) {
    hyprspaces::set_debug_log_sink(capture_debug_message);
    g_debug_message.clear();

    hyprspaces::debug_log(true, "dispatch", "workspace=2");

    EXPECT_TRUE(g_debug_message.starts_with("[hyprspaces][debug] dispatch: workspace=2"));
    EXPECT_NE(g_debug_message.find("logging_test.cpp"), std::string::npos);
    hyprspaces::clear_debug_log_sink();
}

TEST(LoggingFormat, DebugLogNoopWhenDisabled) {
    hyprspaces::set_debug_log_sink(capture_debug_message);
    g_debug_message.clear();

    hyprspaces::debug_log(false, "dispatch", "workspace=2");

    EXPECT_TRUE(g_debug_message.empty());
    hyprspaces::clear_debug_log_sink();
}

TEST(LoggingFormat, LogWithLocationIncludesContextAndFile) {
    const auto text = hyprspaces::format_log_entry_with_location("focus sync", "failed to open config file", std::source_location::current());

    EXPECT_TRUE(text.starts_with("[hyprspaces] focus sync: failed to open config file"));
    EXPECT_NE(text.find("logging_test.cpp"), std::string::npos);
}

TEST(LoggingFormat, ErrorLogUsesSinkWhenEnabled) {
    hyprspaces::set_error_log_sink(capture_error_message);
    g_error_message.clear();

    hyprspaces::error_log("dispatch", "failed to apply", std::source_location::current());

    EXPECT_TRUE(g_error_message.starts_with("[hyprspaces] dispatch: failed to apply"));
    EXPECT_NE(g_error_message.find("logging_test.cpp"), std::string::npos);
    hyprspaces::clear_error_log_sink();
}
