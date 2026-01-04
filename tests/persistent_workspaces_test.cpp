#include <gtest/gtest.h>

#include "hyprspaces/persistent_workspaces.hpp"

TEST(PersistentWorkspaceParse, ParsesNumericWorkspaceIds) {
    const auto first  = hyprspaces::parse_persistent_workspace("3");
    const auto spaced = hyprspaces::parse_persistent_workspace("  7 ");

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 3);
    ASSERT_TRUE(spaced.has_value());
    EXPECT_EQ(*spaced, 7);
}

TEST(PersistentWorkspaceParse, RejectsInvalidWorkspaceIds) {
    EXPECT_EQ(hyprspaces::parse_persistent_workspace(""), std::nullopt);
    EXPECT_EQ(hyprspaces::parse_persistent_workspace("name:code"), std::nullopt);
    EXPECT_EQ(hyprspaces::parse_persistent_workspace("0"), std::nullopt);
    EXPECT_EQ(hyprspaces::parse_persistent_workspace("-2"), std::nullopt);
}
