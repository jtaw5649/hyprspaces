#include <array>
#include <memory>
#include <unordered_map>

#include <gtest/gtest.h>

#include "hyprspaces/layout_restore.hpp"

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
    };

    struct DummyMasterNode {
        bool                       isMaster   = false;
        float                      percMaster = 0.5f;
        std::weak_ptr<DummyWindow> pWindow;
        float                      percSize    = 1.f;
        int                        workspaceID = 0;
    };

} // namespace

TEST(LayoutRestore, BuildsDwindleTree) {
    hyprspaces::LayoutNode root;
    root.split_top        = true;
    root.split_ratio      = 0.25f;
    root.first            = std::make_unique<hyprspaces::LayoutNode>();
    root.first->toplevel  = "left";
    root.second           = std::make_unique<hyprspaces::LayoutNode>();
    root.second->toplevel = "right";

    auto left_window  = std::make_shared<DummyWindow>();
    left_window->id   = 1;
    auto right_window = std::make_shared<DummyWindow>();
    right_window->id  = 2;

    std::unordered_map<std::string, std::shared_ptr<DummyWindow>> windows{
        {"left", left_window},
        {"right", right_window},
    };

    std::vector<std::shared_ptr<DummyNode>> nodes;
    std::string                             error;
    const auto                              built = hyprspaces::layout_restore::build_dwindle_tree<std::shared_ptr<DummyNode>, std::shared_ptr<DummyWindow>>(
        root, 3, nullptr,
        [&](const std::string& name) -> std::optional<std::shared_ptr<DummyWindow>> {
            const auto it = windows.find(name);
            if (it == windows.end()) {
                return std::nullopt;
            }
            return it->second;
        },
        []() { return std::make_shared<DummyNode>(); }, &nodes, &error);

    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(built);
    EXPECT_TRUE(built->isNode);
    EXPECT_EQ(built->workspaceID, 3);
    EXPECT_TRUE(built->splitTop);
    EXPECT_FLOAT_EQ(built->splitRatio, 0.25f);
    ASSERT_TRUE(built->children[0].lock());
    ASSERT_TRUE(built->children[1].lock());
    EXPECT_EQ(built->children[0].lock()->pWindow.lock(), left_window);
    EXPECT_EQ(built->children[1].lock()->pWindow.lock(), right_window);
    EXPECT_EQ(nodes.size(), 3u);
}

TEST(LayoutRestore, BuildsMasterNodes) {
    hyprspaces::MasterWorkspaceSnapshot snapshot;
    snapshot.workspace_id = 8;
    snapshot.nodes.push_back({
        .toplevel    = "term",
        .is_master   = true,
        .perc_master = 0.6f,
        .perc_size   = 1.f,
        .index       = 0,
    });
    snapshot.nodes.push_back({
        .toplevel    = "browser",
        .is_master   = false,
        .perc_master = 0.6f,
        .perc_size   = 0.5f,
        .index       = 1,
    });

    auto                                                          term    = std::make_shared<DummyWindow>();
    auto                                                          browser = std::make_shared<DummyWindow>();
    std::unordered_map<std::string, std::shared_ptr<DummyWindow>> windows{
        {"term", term},
        {"browser", browser},
    };

    std::string error;
    const auto  nodes = hyprspaces::layout_restore::build_master_nodes<DummyMasterNode, std::shared_ptr<DummyWindow>>(
        snapshot,
        [&](const std::string& name) -> std::optional<std::shared_ptr<DummyWindow>> {
            const auto it = windows.find(name);
            if (it == windows.end()) {
                return std::nullopt;
            }
            return it->second;
        },
        &error);

    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(nodes.has_value());
    ASSERT_EQ(nodes->size(), 2u);
    EXPECT_TRUE(nodes->at(0).isMaster);
    EXPECT_EQ(nodes->at(0).workspaceID, 8);
    EXPECT_EQ(nodes->at(0).pWindow.lock(), term);
    EXPECT_FALSE(nodes->at(1).isMaster);
    EXPECT_EQ(nodes->at(1).pWindow.lock(), browser);
}
