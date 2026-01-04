#include <gtest/gtest.h>

#include "hyprspaces/layout_tree.hpp"

namespace {

    hyprspaces::LayoutSnapshot make_dwindle_snapshot() {
        hyprspaces::LayoutSnapshot snapshot;
        snapshot.kind = hyprspaces::LayoutKind::kDwindle;

        hyprspaces::DwindleWorkspaceSnapshot workspace;
        workspace.workspace_id = 3;
        auto root              = std::make_unique<hyprspaces::LayoutNode>();
        root->split_top        = true;
        root->split_ratio      = 0.6f;
        root->first            = std::make_unique<hyprspaces::LayoutNode>();
        root->first->toplevel  = "term";
        root->second           = std::make_unique<hyprspaces::LayoutNode>();
        root->second->toplevel = "browser";
        workspace.root         = std::move(root);
        snapshot.dwindle.push_back(std::move(workspace));

        return snapshot;
    }

    hyprspaces::LayoutSnapshot make_master_snapshot() {
        hyprspaces::LayoutSnapshot snapshot;
        snapshot.kind = hyprspaces::LayoutKind::kMaster;

        hyprspaces::MasterWorkspaceSnapshot workspace;
        workspace.workspace_id = 5;
        workspace.orientation  = hyprspaces::MasterOrientation::kRight;
        workspace.nodes.push_back({
            .toplevel    = "editor",
            .is_master   = true,
            .perc_master = 0.7f,
            .perc_size   = 1.f,
            .index       = 0,
        });
        workspace.nodes.push_back({
            .toplevel    = "browser",
            .is_master   = false,
            .perc_master = 0.7f,
            .perc_size   = 0.5f,
            .index       = 1,
        });
        snapshot.master.push_back(std::move(workspace));

        return snapshot;
    }

} // namespace

TEST(LayoutTreeJson, RoundTripsDwindleSnapshot) {
    const auto  snapshot = make_dwindle_snapshot();
    std::string error;

    const auto  encoded = hyprspaces::layout_snapshot_to_json(snapshot, &error);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_TRUE(error.empty());

    const auto decoded = hyprspaces::layout_snapshot_from_json(encoded->json, &error);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(decoded->kind, hyprspaces::LayoutKind::kDwindle);
    ASSERT_EQ(decoded->dwindle.size(), 1u);
    EXPECT_EQ(decoded->dwindle[0].workspace_id, 3);
    ASSERT_TRUE(decoded->dwindle[0].root);
    EXPECT_TRUE(decoded->dwindle[0].root->split_top);
    EXPECT_FLOAT_EQ(decoded->dwindle[0].root->split_ratio, 0.6f);
    ASSERT_TRUE(decoded->dwindle[0].root->first);
    ASSERT_TRUE(decoded->dwindle[0].root->first->toplevel.has_value());
    EXPECT_EQ(*decoded->dwindle[0].root->first->toplevel, "term");
    ASSERT_TRUE(decoded->dwindle[0].root->second);
    ASSERT_TRUE(decoded->dwindle[0].root->second->toplevel.has_value());
    EXPECT_EQ(*decoded->dwindle[0].root->second->toplevel, "browser");
}

TEST(LayoutTreeJson, RoundTripsMasterSnapshot) {
    const auto  snapshot = make_master_snapshot();
    std::string error;

    const auto  encoded = hyprspaces::layout_snapshot_to_json(snapshot, &error);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_TRUE(error.empty());

    const auto decoded = hyprspaces::layout_snapshot_from_json(encoded->json, &error);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(decoded->kind, hyprspaces::LayoutKind::kMaster);
    ASSERT_EQ(decoded->master.size(), 1u);
    EXPECT_EQ(decoded->master[0].workspace_id, 5);
    EXPECT_EQ(decoded->master[0].orientation, hyprspaces::MasterOrientation::kRight);
    ASSERT_EQ(decoded->master[0].nodes.size(), 2u);
    EXPECT_EQ(decoded->master[0].nodes[0].toplevel, "editor");
    EXPECT_TRUE(decoded->master[0].nodes[0].is_master);
    EXPECT_FLOAT_EQ(decoded->master[0].nodes[0].perc_master, 0.7f);
    EXPECT_FLOAT_EQ(decoded->master[0].nodes[0].perc_size, 1.f);
    EXPECT_EQ(decoded->master[0].nodes[0].index, 0);
    EXPECT_EQ(decoded->master[0].nodes[1].toplevel, "browser");
    EXPECT_FALSE(decoded->master[0].nodes[1].is_master);
    EXPECT_FLOAT_EQ(decoded->master[0].nodes[1].perc_master, 0.7f);
    EXPECT_FLOAT_EQ(decoded->master[0].nodes[1].perc_size, 0.5f);
    EXPECT_EQ(decoded->master[0].nodes[1].index, 1);
}

TEST(LayoutTreeJson, RejectsInvalidJson) {
    std::string error;

    const auto  decoded = hyprspaces::layout_snapshot_from_json("{not-json", &error);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "layout snapshot invalid json");
}
