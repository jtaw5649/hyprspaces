#include "hyprspaces/strings.hpp"

#include <gtest/gtest.h>

namespace {

    TEST(TrimView, TrimsLeadingWhitespace) {
        EXPECT_EQ(hyprspaces::trim_view("  hello"), "hello");
    }

    TEST(TrimView, TrimsTrailingWhitespace) {
        EXPECT_EQ(hyprspaces::trim_view("hello  "), "hello");
    }

    TEST(TrimView, TrimsBothEnds) {
        EXPECT_EQ(hyprspaces::trim_view("  hello  "), "hello");
    }

    TEST(TrimView, HandlesEmptyString) {
        EXPECT_EQ(hyprspaces::trim_view(""), "");
    }

    TEST(TrimView, ReturnsEmptyForWhitespaceOnly) {
        EXPECT_EQ(hyprspaces::trim_view("   "), "");
    }

    TEST(TrimCopy, ReturnsTrimmedCopy) {
        std::string result = hyprspaces::trim_copy("  hello  ");
        EXPECT_EQ(result, "hello");
    }

}
