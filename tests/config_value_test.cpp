#include <any>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "hyprspaces/config_value.hpp"

namespace {

    TEST(ConfigValueView, StringOverrideRequiresUserSet) {
        hyprspaces::ConfigValueView view{.set_by_user = false, .value = std::any(static_cast<const char*>("DP-1"))};

        EXPECT_EQ(hyprspaces::read_string_override(view), std::nullopt);
    }

    TEST(ConfigValueView, StringOverrideNormalizesWhitespace) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(static_cast<const char*>("  DP-1 "))};

        EXPECT_EQ(hyprspaces::read_string_override(view), std::optional<std::string>{"DP-1"});
    }

    TEST(ConfigValueView, StringOverrideRejectsNonStringTypes) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(int64_t{5})};

        EXPECT_EQ(hyprspaces::read_string_override(view), std::nullopt);
    }

    TEST(ConfigValueView, PositiveIntOverrideRejectsZero) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(int64_t{0})};

        EXPECT_EQ(hyprspaces::read_positive_int_override(view), std::nullopt);
    }

    TEST(ConfigValueView, PositiveIntOverrideAcceptsPositive) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(int64_t{7})};

        EXPECT_EQ(hyprspaces::read_positive_int_override(view), std::optional<int>{7});
    }

    TEST(ConfigValueView, NonNegativeIntOverrideAcceptsZero) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(int64_t{0})};

        EXPECT_EQ(hyprspaces::read_non_negative_int_override(view), std::optional<int>{0});
    }

    TEST(ConfigValueView, BoolOverrideAcceptsInt) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(int64_t{0})};

        EXPECT_EQ(hyprspaces::read_bool_override(view), std::optional<bool>{false});
    }

    TEST(ConfigValueView, BoolOverrideAcceptsBool) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(true)};

        EXPECT_EQ(hyprspaces::read_bool_override(view), std::optional<bool>{true});
    }

    TEST(ConfigValueView, BoolValueDefaultsOnInvalidType) {
        hyprspaces::ConfigValueView view{.set_by_user = true, .value = std::any(std::string("nope"))};

        EXPECT_TRUE(hyprspaces::read_bool_value(view, true));
    }

} // namespace
