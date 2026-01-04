#include "hyprspaces/json_utils.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

    TEST(OptionalStringField, ReturnsNulloptForMissingKey) {
        nlohmann::json obj    = {{"other", "value"}};
        auto           result = hyprspaces::optional_string_field(obj, "missing");
        EXPECT_FALSE(result.has_value());
    }

    TEST(OptionalStringField, ReturnsNulloptForNullValue) {
        nlohmann::json obj    = {{"key", nullptr}};
        auto           result = hyprspaces::optional_string_field(obj, "key");
        EXPECT_FALSE(result.has_value());
    }

    TEST(OptionalStringField, ReturnsValueForValidKey) {
        nlohmann::json obj    = {{"name", "test"}};
        auto           result = hyprspaces::optional_string_field(obj, "name");
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "test");
    }

    TEST(OptionalIntField, ReturnsNulloptForMissingKey) {
        nlohmann::json obj    = {{"other", 42}};
        auto           result = hyprspaces::optional_int_field(obj, "missing");
        EXPECT_FALSE(result.has_value());
    }

    TEST(OptionalIntField, ReturnsValueForValidKey) {
        nlohmann::json obj    = {{"pid", 12345}};
        auto           result = hyprspaces::optional_int_field(obj, "pid");
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 12345);
    }

}
