#include <gtest/gtest.h>

#include "hyprspaces/layout_restore_tracker.hpp"

namespace {

    hyprspaces::LayoutSnapshot make_dwindle_snapshot() {
        hyprspaces::LayoutSnapshot snapshot;
        snapshot.kind = hyprspaces::LayoutKind::kDwindle;

        hyprspaces::DwindleWorkspaceSnapshot workspace;
        workspace.workspace_id = 2;
        auto root              = std::make_unique<hyprspaces::LayoutNode>();
        root->split_top        = true;
        root->split_ratio      = 0.4f;
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
        workspace.workspace_id = 6;
        workspace.orientation  = hyprspaces::MasterOrientation::kLeft;
        workspace.nodes.push_back({
            .toplevel    = "editor",
            .is_master   = true,
            .perc_master = 0.5f,
            .perc_size   = 1.0f,
            .index       = 0,
        });
        workspace.nodes.push_back({
            .toplevel    = "",
            .is_master   = false,
            .perc_master = 0.5f,
            .perc_size   = 0.5f,
            .index       = 1,
        });
        workspace.nodes.push_back({
            .toplevel    = "chat",
            .is_master   = false,
            .perc_master = 0.5f,
            .perc_size   = 0.5f,
            .index       = 2,
        });
        snapshot.master.push_back(std::move(workspace));

        return snapshot;
    }

} // namespace

TEST(LayoutRestoreTracker, EmitsReadyForDwindle) {
    hyprspaces::LayoutRestoreTracker<int> tracker;
    tracker.set_snapshot("session", make_dwindle_snapshot());

    EXPECT_TRUE(tracker.has_pending("session"));
    EXPECT_FALSE(tracker.record_window("session", "unknown", 9).has_value());

    EXPECT_FALSE(tracker.record_window("session", "term", 1).has_value());
    auto ready = tracker.record_window("session", "browser", 2);
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ(ready->snapshot.kind, hyprspaces::LayoutKind::kDwindle);
    EXPECT_EQ(ready->windows.size(), 2u);
    EXPECT_EQ(ready->windows["term"], 1);
    EXPECT_EQ(ready->windows["browser"], 2);
    EXPECT_FALSE(tracker.has_pending("session"));
}

TEST(LayoutRestoreTracker, EmitsReadyForMaster) {
    hyprspaces::LayoutRestoreTracker<int> tracker;
    tracker.set_snapshot("session", make_master_snapshot());

    EXPECT_TRUE(tracker.has_pending("session"));
    EXPECT_FALSE(tracker.record_window("session", "chat", 7).has_value());
    auto ready = tracker.record_window("session", "editor", 3);
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ(ready->snapshot.kind, hyprspaces::LayoutKind::kMaster);
    EXPECT_EQ(ready->windows.size(), 2u);
    EXPECT_EQ(ready->windows["editor"], 3);
    EXPECT_EQ(ready->windows["chat"], 7);
}

TEST(LayoutRestoreTracker, ClearsPendingSession) {
    hyprspaces::LayoutRestoreTracker<int> tracker;
    tracker.set_snapshot("session", make_dwindle_snapshot());
    EXPECT_TRUE(tracker.has_pending("session"));

    tracker.clear_session("session");
    EXPECT_FALSE(tracker.has_pending("session"));
    EXPECT_FALSE(tracker.record_window("session", "term", 1).has_value());
}
