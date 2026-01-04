#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "hyprspaces/layout_tree.hpp"
#include "hyprspaces/session_protocol_store.hpp"
#include "hyprspaces/types.hpp"

namespace {

    std::filesystem::path make_temp_dir() {
        const auto base   = std::filesystem::temp_directory_path();
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto       dir    = base / ("hyprspaces_session_store_" + unique);
        std::filesystem::create_directories(dir);
        return dir;
    }

    hyprspaces::LayoutSnapshot make_layout_snapshot() {
        hyprspaces::LayoutSnapshot snapshot;
        snapshot.kind = hyprspaces::LayoutKind::kDwindle;

        hyprspaces::DwindleWorkspaceSnapshot workspace;
        workspace.workspace_id = 4;
        auto root              = std::make_unique<hyprspaces::LayoutNode>();
        root->split_top        = false;
        root->split_ratio      = 0.5f;
        root->first            = std::make_unique<hyprspaces::LayoutNode>();
        root->first->toplevel  = "editor";
        root->second           = std::make_unique<hyprspaces::LayoutNode>();
        root->second->toplevel = "browser";
        workspace.root         = std::move(root);
        snapshot.dwindle.push_back(std::move(workspace));

        return snapshot;
    }

} // namespace

TEST(SessionProtocolStore, LoadsMissingFileAsEmpty) {
    const auto  dir  = make_temp_dir();
    const auto  path = dir / "session-store.json";
    std::string error;

    const auto  loaded = hyprspaces::SessionProtocolStore::load(path, &error);

    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(loaded->has_session("missing"));
    std::filesystem::remove_all(dir);
}

TEST(SessionProtocolStore, RejectsInvalidJson) {
    const auto  dir  = make_temp_dir();
    const auto  path = dir / "session-store.json";
    std::string error;

    {
        std::ofstream output(path);
        output << "{not-json";
    }

    const auto loaded = hyprspaces::SessionProtocolStore::load(path, &error);

    EXPECT_FALSE(loaded.has_value());
    EXPECT_EQ(error, "invalid session store");
    std::filesystem::remove_all(dir);
}

TEST(SessionProtocolStore, SavesAndReloadsSessions) {
    const auto                       dir  = make_temp_dir();
    const auto                       path = dir / "state" / "session-store.json";
    std::string                      error;

    hyprspaces::SessionProtocolStore store;
    EXPECT_TRUE(store.remember_session("session-1"));
    EXPECT_TRUE(store.add_toplevel("session-1", "term"));
    EXPECT_TRUE(store.add_toplevel("session-1", "browser"));

    EXPECT_TRUE(store.save(path, &error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(std::filesystem::exists(path));

    const auto loaded = hyprspaces::SessionProtocolStore::load(path, &error);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(loaded->has_session("session-1"));
    EXPECT_EQ(loaded->session_ids(), std::vector<std::string>({"session-1"}));
    const auto toplevels = loaded->toplevels_for("session-1");
    ASSERT_TRUE(toplevels.has_value());
    ASSERT_EQ(toplevels->size(), 2u);
    EXPECT_EQ(toplevels->at(0).name, "term");
    EXPECT_EQ(toplevels->at(1).name, "browser");
    std::filesystem::remove_all(dir);
}

TEST(SessionProtocolStore, RemovesSessionsAndToplevels) {
    hyprspaces::SessionProtocolStore store;

    EXPECT_TRUE(store.remember_session("session-1"));
    EXPECT_TRUE(store.add_toplevel("session-1", "term"));
    EXPECT_TRUE(store.remove_toplevel("session-1", "term"));
    EXPECT_FALSE(store.remove_toplevel("session-1", "term"));
    EXPECT_TRUE(store.remove_session("session-1"));
    EXPECT_FALSE(store.has_session("session-1"));
}

TEST(SessionProtocolStore, UpdatesToplevelState) {
    hyprspaces::SessionProtocolStore store;

    EXPECT_TRUE(store.remember_session("session-1"));
    EXPECT_TRUE(store.add_toplevel("session-1", "term"));

    hyprspaces::SessionProtocolStore::ToplevelState state{
        .name       = "term",
        .geometry   = hyprspaces::WindowGeometry{.x = 10, .y = 20, .width = 300, .height = 400},
        .floating   = true,
        .pinned     = false,
        .fullscreen = hyprspaces::FullscreenState{.internal = 1, .client = 0},
    };

    EXPECT_TRUE(store.update_toplevel_state("session-1", "term", state));
    const auto toplevels = store.toplevels_for("session-1");
    ASSERT_TRUE(toplevels.has_value());
    ASSERT_EQ(toplevels->size(), 1u);
    ASSERT_TRUE(toplevels->at(0).geometry.has_value());
    EXPECT_EQ(toplevels->at(0).geometry->x, 10);
    EXPECT_EQ(toplevels->at(0).geometry->y, 20);
    EXPECT_EQ(toplevels->at(0).geometry->width, 300);
    EXPECT_EQ(toplevels->at(0).geometry->height, 400);
    ASSERT_TRUE(toplevels->at(0).floating.has_value());
    EXPECT_TRUE(*toplevels->at(0).floating);
    ASSERT_TRUE(toplevels->at(0).pinned.has_value());
    EXPECT_FALSE(*toplevels->at(0).pinned);
    ASSERT_TRUE(toplevels->at(0).fullscreen.has_value());
    EXPECT_EQ(toplevels->at(0).fullscreen->internal, 1);
    EXPECT_EQ(toplevels->at(0).fullscreen->client, 0);
}

TEST(SessionProtocolStore, SavesAndReloadsLayoutSnapshot) {
    const auto                       dir  = make_temp_dir();
    const auto                       path = dir / "session-store.json";
    std::string                      error;

    hyprspaces::SessionProtocolStore store;
    EXPECT_TRUE(store.remember_session("session-1"));
    EXPECT_TRUE(store.update_layout_snapshot("session-1", make_layout_snapshot()));
    EXPECT_FALSE(store.update_layout_snapshot("missing-session", make_layout_snapshot()));

    EXPECT_TRUE(store.save(path, &error));
    EXPECT_TRUE(error.empty());

    const auto loaded = hyprspaces::SessionProtocolStore::load(path, &error);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(error.empty());

    const auto layout = loaded->layout_for("session-1");
    ASSERT_TRUE(layout.has_value());
    EXPECT_EQ(layout->kind, hyprspaces::LayoutKind::kDwindle);
    ASSERT_EQ(layout->dwindle.size(), 1u);
    EXPECT_EQ(layout->dwindle[0].workspace_id, 4);
    ASSERT_TRUE(layout->dwindle[0].root);
    EXPECT_FALSE(layout->dwindle[0].root->split_top);
    EXPECT_FLOAT_EQ(layout->dwindle[0].root->split_ratio, 0.5f);
    ASSERT_TRUE(layout->dwindle[0].root->first);
    ASSERT_TRUE(layout->dwindle[0].root->first->toplevel.has_value());
    EXPECT_EQ(*layout->dwindle[0].root->first->toplevel, "editor");
    ASSERT_TRUE(layout->dwindle[0].root->second);
    ASSERT_TRUE(layout->dwindle[0].root->second->toplevel.has_value());
    EXPECT_EQ(*layout->dwindle[0].root->second->toplevel, "browser");

    std::filesystem::remove_all(dir);
}
