#include <gtest/gtest.h>

#include "hyprspaces/hyprctl.hpp"

TEST(HyprctlParse, ActiveWorkspaceId) {
    const auto id = hyprspaces::parse_active_workspace_id(R"({"id":42,"name":"42"})");

    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 42);
}

TEST(HyprctlParse, ActiveWindowAddress) {
    const auto address = hyprspaces::parse_active_window_address(R"({"address":"0x123","mapped":true})");

    ASSERT_TRUE(address.has_value());
    EXPECT_EQ(*address, "0x123");
}

TEST(HyprctlParse, InvalidJsonReturnsError) {
    const auto result = hyprspaces::parse_active_workspace_id("{not-json");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().context, "activeworkspace");
    EXPECT_EQ(result.error().message, "invalid json");
}

TEST(HyprctlParse, Monitors) {
    const auto monitors = hyprspaces::parse_monitors(R"([{"name":"DP-1","id":1,"x":0,"description":"Dell","activeWorkspace":{"id":3,"name":"3"}},)"
                                                     R"({"name":"HDMI-A-1","id":2,"x":1920}])");

    ASSERT_TRUE(monitors.has_value());
    ASSERT_EQ(monitors->size(), 2u);
    EXPECT_EQ((*monitors)[0].name, "DP-1");
    EXPECT_EQ((*monitors)[0].id, 1);
    EXPECT_EQ((*monitors)[0].description.value_or(""), "Dell");
    EXPECT_EQ((*monitors)[0].active_workspace_id.value_or(0), 3);
    EXPECT_EQ((*monitors)[1].x, 1920);
    EXPECT_FALSE((*monitors)[1].description.has_value());
    EXPECT_FALSE((*monitors)[1].active_workspace_id.has_value());
}

TEST(HyprctlParse, Workspaces) {
    const auto workspaces = hyprspaces::parse_workspaces(R"([{"id":1,"windows":2,"name":"1","monitor":"DP-1"},)"
                                                         R"({"id":12,"windows":0,"name":"12","monitor":"HDMI-A-1"}])");

    ASSERT_TRUE(workspaces.has_value());
    ASSERT_EQ(workspaces->size(), 2u);
    EXPECT_EQ((*workspaces)[0].id, 1);
    EXPECT_EQ((*workspaces)[0].windows, 2);
    EXPECT_EQ((*workspaces)[1].monitor.value_or(""), "HDMI-A-1");
}

TEST(HyprctlParse, Clients) {
    const auto clients = hyprspaces::parse_clients(R"([{"address":"0x123","workspace":{"id":12,"name":"12"},)"
                                                   R"("class":"kitty","title":"term","initialClass":"kitty",)"
                                                   R"("initialTitle":"term","pid":4242,)"
                                                   R"("at":[10,20],"size":[800,600],"floating":true,"pinned":false,)"
                                                   R"("fullscreen":1,"fullscreenClient":1}])");

    ASSERT_TRUE(clients.has_value());
    ASSERT_EQ(clients->size(), 1u);
    EXPECT_EQ((*clients)[0].address, "0x123");
    EXPECT_EQ((*clients)[0].workspace.id, 12);
    EXPECT_EQ((*clients)[0].class_name.value_or(""), "kitty");
    EXPECT_EQ((*clients)[0].initial_class.value_or(""), "kitty");
    EXPECT_EQ((*clients)[0].pid.value_or(0), 4242);
    ASSERT_TRUE((*clients)[0].geometry.has_value());
    EXPECT_EQ((*clients)[0].geometry->x, 10);
    EXPECT_EQ((*clients)[0].geometry->y, 20);
    EXPECT_EQ((*clients)[0].geometry->width, 800);
    EXPECT_EQ((*clients)[0].geometry->height, 600);
    ASSERT_TRUE((*clients)[0].floating.has_value());
    EXPECT_TRUE(*(*clients)[0].floating);
    ASSERT_TRUE((*clients)[0].pinned.has_value());
    EXPECT_FALSE(*(*clients)[0].pinned);
    ASSERT_TRUE((*clients)[0].fullscreen.has_value());
    EXPECT_EQ((*clients)[0].fullscreen->internal, 1);
    EXPECT_EQ((*clients)[0].fullscreen->client, 1);
}

TEST(HyprctlParse, OkResponse) {
    EXPECT_TRUE(hyprspaces::is_ok_response("ok"));
    EXPECT_TRUE(hyprspaces::is_ok_response("ok\n\n\nok"));
    EXPECT_TRUE(hyprspaces::is_ok_response("ok\n\n\n"));
    EXPECT_FALSE(hyprspaces::is_ok_response("unknown request"));
    EXPECT_FALSE(hyprspaces::is_ok_response("invalid dispatcher"));
}
