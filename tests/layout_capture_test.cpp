#include <array>
#include <memory>
#include <unordered_map>

#include <gtest/gtest.h>

#include "hyprspaces/layout_capture.hpp"

namespace {

    struct DummyWindow {
        int id = 0;
    };

    struct DummyNode {
        std::weak_ptr<DummyNode>                pParent;
        bool                                    isNode = false;
        std::weak_ptr<DummyWindow>              pWindow;
        std::array<std::weak_ptr<DummyNode>, 2> children    = {};
        int                                     workspaceID = 0;
        float                                   splitRatio  = 1.f;
        bool                                    splitTop    = false;
        bool                                    valid       = true;
    };

    struct DummyMasterNode {
        bool                       isMaster   = false;
        float                      percMaster = 0.5f;
        std::weak_ptr<DummyWindow> pWindow;
        float                      percSize    = 1.f;
        int                        workspaceID = 0;
    };

    struct DummyMasterWorkspace {
        int workspaceID = 0;
        int orientation = 0;
    };

} // namespace

TEST(LayoutCapture, CapturesDwindleTree) {
    auto win_left  = std::make_shared<DummyWindow>();
    win_left->id   = 1;
    auto win_right = std::make_shared<DummyWindow>();
    win_right->id  = 2;

    auto root         = std::make_shared<DummyNode>();
    root->workspaceID = 2;
    root->splitTop    = true;
    root->splitRatio  = 0.4f;
    root->isNode      = true;

    auto left         = std::make_shared<DummyNode>();
    left->workspaceID = 2;
    left->pParent     = root;
    left->pWindow     = win_left;
    left->isNode      = false;

    auto right         = std::make_shared<DummyNode>();
    right->workspaceID = 2;
    right->pParent     = root;
    right->pWindow     = win_right;
    right->isNode      = false;

    root->children = {left, right};

    std::vector<std::shared_ptr<DummyNode>>             nodes{root, left, right};
    std::unordered_map<const DummyWindow*, std::string> names{
        {win_left.get(), "term"},
        {win_right.get(), "browser"},
    };

    std::string error;
    const auto  snapshots = hyprspaces::layout_capture::capture_dwindle_workspaces(
        nodes,
        [&](const std::shared_ptr<DummyWindow>& window) -> std::optional<std::string> {
            const auto it = names.find(window.get());
            if (it == names.end()) {
                return std::nullopt;
            }
            return it->second;
        },
        &error);

    ASSERT_TRUE(error.empty());
    ASSERT_EQ(snapshots.size(), 1u);
    EXPECT_EQ(snapshots[0].workspace_id, 2);
    ASSERT_TRUE(snapshots[0].root);
    EXPECT_TRUE(snapshots[0].root->split_top);
    EXPECT_FLOAT_EQ(snapshots[0].root->split_ratio, 0.4f);
    ASSERT_TRUE(snapshots[0].root->first);
    ASSERT_TRUE(snapshots[0].root->first->toplevel.has_value());
    EXPECT_EQ(*snapshots[0].root->first->toplevel, "term");
    ASSERT_TRUE(snapshots[0].root->second);
    ASSERT_TRUE(snapshots[0].root->second->toplevel.has_value());
    EXPECT_EQ(*snapshots[0].root->second->toplevel, "browser");
}

TEST(LayoutCapture, CapturesMasterNodes) {
    auto win_master = std::make_shared<DummyWindow>();
    win_master->id  = 3;
    auto win_stack  = std::make_shared<DummyWindow>();
    win_stack->id   = 4;

    std::list<DummyMasterNode> nodes;
    nodes.push_back({
        .isMaster    = true,
        .percMaster  = 0.6f,
        .pWindow     = win_master,
        .percSize    = 1.f,
        .workspaceID = 9,
    });
    nodes.push_back({
        .isMaster    = false,
        .percMaster  = 0.6f,
        .pWindow     = win_stack,
        .percSize    = 0.5f,
        .workspaceID = 9,
    });

    std::vector<DummyMasterWorkspace> workspaces{
        {.workspaceID = 9, .orientation = 2},
    };

    std::unordered_map<const DummyWindow*, std::string> names{
        {win_master.get(), "editor"},
        {win_stack.get(), "browser"},
    };

    const auto snapshots = hyprspaces::layout_capture::capture_master_workspaces(
        nodes, workspaces,
        [&](const std::shared_ptr<DummyWindow>& window) -> std::optional<std::string> {
            const auto it = names.find(window.get());
            if (it == names.end()) {
                return std::nullopt;
            }
            return it->second;
        },
        [](const DummyMasterWorkspace& workspace) { return workspace.orientation == 2 ? hyprspaces::MasterOrientation::kRight : hyprspaces::MasterOrientation::kLeft; });

    ASSERT_EQ(snapshots.size(), 1u);
    EXPECT_EQ(snapshots[0].workspace_id, 9);
    EXPECT_EQ(snapshots[0].orientation, hyprspaces::MasterOrientation::kRight);
    ASSERT_EQ(snapshots[0].nodes.size(), 2u);
    EXPECT_EQ(snapshots[0].nodes[0].toplevel, "editor");
    EXPECT_TRUE(snapshots[0].nodes[0].is_master);
    EXPECT_EQ(snapshots[0].nodes[0].index, 0);
    EXPECT_EQ(snapshots[0].nodes[1].toplevel, "browser");
    EXPECT_FALSE(snapshots[0].nodes[1].is_master);
    EXPECT_EQ(snapshots[0].nodes[1].index, 1);
}
