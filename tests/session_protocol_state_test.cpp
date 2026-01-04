#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "hyprspaces/layout_tree.hpp"
#include "hyprspaces/session_protocol_state.hpp"
#include "hyprspaces/types.hpp"

namespace {

    std::filesystem::path make_temp_dir() {
        const auto base   = std::filesystem::temp_directory_path();
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto       dir    = base / ("hyprspaces_session_state_" + unique);
        std::filesystem::create_directories(dir);
        return dir;
    }

} // namespace

TEST(SessionProtocolState, LoadsStoreAndRestoresSession) {
    const auto                       dir  = make_temp_dir();
    const auto                       path = dir / "session-store.json";

    hyprspaces::SessionProtocolStore store;
    std::string                      error;
    EXPECT_TRUE(store.remember_session("session-1"));
    EXPECT_TRUE(store.add_toplevel("session-1", "term"));
    EXPECT_TRUE(store.save(path, &error));

    hyprspaces::SessionProtocolState state([]() { return std::string("session-new"); });
    EXPECT_TRUE(state.load_store(path, &error));

    const auto result = state.open_session(reinterpret_cast<void*>(0x1), std::string_view("session-1"), hyprspaces::SessionReason::kRecover, &error);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result->state, hyprspaces::SessionOpenState::kRestored);
    std::filesystem::remove_all(dir);
}

TEST(SessionProtocolState, CreatesSessionEntriesOnOpen) {
    hyprspaces::SessionProtocolState state([]() { return std::string("session-1"); });
    std::string                      error;

    const auto                       result = state.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(result->id, "session-1");
    EXPECT_TRUE(state.store().has_session("session-1"));
}

TEST(SessionProtocolState, TracksToplevelNamesPerSession) {
    hyprspaces::SessionProtocolState state([]() { return std::string("session-1"); });
    std::string                      error;

    const auto                       result = state.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(state.add_toplevel(result->id, "term", &error));
    EXPECT_FALSE(state.add_toplevel(result->id, "term", &error));
    EXPECT_TRUE(state.remove_toplevel(result->id, "term"));
    EXPECT_FALSE(state.remove_toplevel(result->id, "term"));
}

TEST(SessionProtocolState, ReleasesToplevelNameWithoutRemovingStoreEntry) {
    hyprspaces::SessionProtocolState state([]() { return std::string("session-1"); });
    std::string                      error;

    const auto                       result = state.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(state.add_toplevel(result->id, "term", &error));
    state.release_toplevel(result->id, "term");

    error.clear();
    EXPECT_TRUE(state.add_toplevel(result->id, "term", &error));
    const auto toplevels = state.store().toplevels_for(result->id);
    ASSERT_TRUE(toplevels.has_value());
    ASSERT_EQ(toplevels->size(), 1u);
    EXPECT_EQ(toplevels->at(0).name, "term");
}

TEST(SessionProtocolState, UpdatesToplevelState) {
    hyprspaces::SessionProtocolState state([]() { return std::string("session-1"); });
    std::string                      error;

    const auto                       result = state.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(state.add_toplevel(result->id, "term", &error));

    hyprspaces::SessionProtocolStore::ToplevelState snapshot{
        .name       = "term",
        .geometry   = hyprspaces::WindowGeometry{.x = 5, .y = 6, .width = 7, .height = 8},
        .floating   = false,
        .pinned     = true,
        .fullscreen = hyprspaces::FullscreenState{.internal = 0, .client = 1},
    };

    EXPECT_TRUE(state.update_toplevel_state(result->id, "term", snapshot));
    const auto toplevels = state.store().toplevels_for(result->id);
    ASSERT_TRUE(toplevels.has_value());
    ASSERT_EQ(toplevels->size(), 1u);
    ASSERT_TRUE(toplevels->at(0).geometry.has_value());
    EXPECT_EQ(toplevels->at(0).geometry->x, 5);
    EXPECT_EQ(toplevels->at(0).geometry->y, 6);
    EXPECT_EQ(toplevels->at(0).geometry->width, 7);
    EXPECT_EQ(toplevels->at(0).geometry->height, 8);
    ASSERT_TRUE(toplevels->at(0).floating.has_value());
    EXPECT_FALSE(*toplevels->at(0).floating);
    ASSERT_TRUE(toplevels->at(0).pinned.has_value());
    EXPECT_TRUE(*toplevels->at(0).pinned);
    ASSERT_TRUE(toplevels->at(0).fullscreen.has_value());
    EXPECT_EQ(toplevels->at(0).fullscreen->internal, 0);
    EXPECT_EQ(toplevels->at(0).fullscreen->client, 1);
}

TEST(SessionProtocolState, UpdatesLayoutSnapshot) {
    hyprspaces::SessionProtocolState state([]() { return std::string("session-1"); });
    std::string                      error;

    const auto                       result = state.open_session(reinterpret_cast<void*>(0x1), std::nullopt, hyprspaces::SessionReason::kLaunch, &error);
    ASSERT_TRUE(result.has_value());

    hyprspaces::LayoutSnapshot snapshot;
    snapshot.kind = hyprspaces::LayoutKind::kMaster;
    hyprspaces::MasterWorkspaceSnapshot workspace;
    workspace.workspace_id = 7;
    workspace.orientation  = hyprspaces::MasterOrientation::kTop;
    workspace.nodes.push_back({
        .toplevel    = "terminal",
        .is_master   = true,
        .perc_master = 0.6f,
        .perc_size   = 1.f,
        .index       = 0,
    });
    snapshot.master.push_back(std::move(workspace));

    EXPECT_TRUE(state.update_layout_snapshot(result->id, snapshot));
    const auto stored = state.store().layout_for(result->id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->kind, hyprspaces::LayoutKind::kMaster);
    ASSERT_EQ(stored->master.size(), 1u);
    EXPECT_EQ(stored->master[0].workspace_id, 7);
    EXPECT_EQ(stored->master[0].orientation, hyprspaces::MasterOrientation::kTop);
    ASSERT_EQ(stored->master[0].nodes.size(), 1u);
    EXPECT_EQ(stored->master[0].nodes[0].toplevel, "terminal");
}
