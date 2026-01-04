#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <sys/wait.h>

#include <gtest/gtest.h>

namespace {

    struct CommandResult {
        std::string output;
        int         exit_status = -1;
    };

    CommandResult run_command(const std::string& command) {
        CommandResult         result;
        std::array<char, 256> buffer{};
        std::FILE*            pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return result;
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            result.output.append(buffer.data());
        }

        const int status = pclose(pipe);
        if (status != -1 && WIFEXITED(status)) {
            result.exit_status = WEXITSTATUS(status);
        }

        return result;
    }

    std::filesystem::path script_path() {
        return std::filesystem::path(__FILE__).parent_path() / ".." / "scripts" / "plugin-install.sh";
    }

} // namespace

TEST(PluginInstallScript, HelpMentionsCommands) {
    const auto path = script_path();
    ASSERT_TRUE(std::filesystem::exists(path)) << "Missing script: " << path.string();

    const auto result = run_command("bash \"" + path.string() + "\" --help");

    ASSERT_EQ(result.exit_status, 0);
    EXPECT_NE(result.output.find("install"), std::string::npos);
    EXPECT_NE(result.output.find("enable"), std::string::npos);
    EXPECT_NE(result.output.find("disable"), std::string::npos);
    EXPECT_NE(result.output.find("remove"), std::string::npos);
}
