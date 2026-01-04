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

TEST(FunctionHooks, SessionProtocolInstallsWindowMapHook) {
    const auto source_path = std::filesystem::path(__FILE__).parent_path() / ".." / "src" / "session_protocol_server.cpp";
    const auto contents    = read_file(source_path);

    ASSERT_FALSE(contents.empty());
    EXPECT_NE(contents.find("onMap"), std::string::npos);
    EXPECT_NE(contents.find("createFunctionHook"), std::string::npos);
}

TEST(FunctionHooks, SessionProtocolInstallsWindowStateHook) {
    const auto source_path = std::filesystem::path(__FILE__).parent_path() / ".." / "src" / "session_protocol_server.cpp";
    const auto contents    = read_file(source_path);

    ASSERT_FALSE(contents.empty());
    EXPECT_NE(contents.find("onUpdateState"), std::string::npos);
    EXPECT_NE(contents.find("createFunctionHook"), std::string::npos);
}
