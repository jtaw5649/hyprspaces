#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

namespace {

    std::string read_file(const std::filesystem::path& path) {
        std::ifstream      input(path);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

} // namespace

TEST(PluginEntry, IsWiringOnly) {
    const auto plugin_path = std::filesystem::path(__FILE__).parent_path() / ".." / "src" / "plugin.cpp";
    const auto contents    = read_file(plugin_path);

    ASSERT_FALSE(contents.empty());
    EXPECT_NE(contents.find("PLUGIN_API_VERSION"), std::string::npos);
    EXPECT_NE(contents.find("PLUGIN_INIT"), std::string::npos);
    EXPECT_NE(contents.find("PLUGIN_EXIT"), std::string::npos);
    EXPECT_EQ(contents.find("namespace {"), std::string::npos);
    EXPECT_EQ(contents.find("g_state"), std::string::npos);
    EXPECT_EQ(contents.find("g_invoker"), std::string::npos);
    EXPECT_EQ(contents.find("Hyprlang::"), std::string::npos);
    EXPECT_EQ(contents.find("HyprlandAPI::"), std::string::npos);
}

TEST(PluginEntry, AvoidsSessionRestoreInEventHooks) {
    const auto plugin_path = std::filesystem::path(__FILE__).parent_path() / ".." / "src" / "plugin_entry.cpp";
    const auto contents    = read_file(plugin_path);

    ASSERT_FALSE(contents.empty());
    EXPECT_EQ(contents.find("maybe_restore_window_state"), std::string::npos);
}
