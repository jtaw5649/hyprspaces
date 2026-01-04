#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>

#include <gtest/gtest.h>

#include "hyprspaces/process_utils.hpp"

namespace {

    std::filesystem::path make_proc_root() {
        const auto         now = std::chrono::steady_clock::now().time_since_epoch().count();
        std::random_device rd;
        return std::filesystem::temp_directory_path() / (std::string("hyprspaces-proc-") + std::to_string(now) + "-" + std::to_string(rd()));
    }

    void write_cmdline(const std::filesystem::path& proc_root, int pid, const std::string& contents) {
        const auto dir = proc_root / std::to_string(pid);
        std::filesystem::create_directories(dir);
        std::ofstream output(dir / "cmdline", std::ios::binary);
        output << contents;
    }

} // namespace

TEST(ProcessUtils, ReadsNullSeparatedCmdline) {
    const auto proc_root = make_proc_root();
    write_cmdline(proc_root, 4242, std::string("alacritty\0-e\0nvim\0.\0", 20));

    const auto cmdline = hyprspaces::read_process_cmdline(4242, proc_root);

    ASSERT_TRUE(cmdline.has_value());
    EXPECT_EQ(*cmdline, "alacritty -e nvim .");
}

TEST(ProcessUtils, FillsClientCommandsFromProc) {
    const auto proc_root = make_proc_root();
    write_cmdline(proc_root, 8080, std::string("kitty\0--class\0term\0", 19));

    std::vector<hyprspaces::ClientInfo> clients{
        {.address       = "0x1",
         .workspace     = {.id = 1, .name = std::nullopt},
         .class_name    = std::nullopt,
         .title         = std::nullopt,
         .initial_class = std::nullopt,
         .initial_title = std::nullopt,
         .app_id        = std::nullopt,
         .pid           = 8080,
         .command       = std::nullopt,
         .geometry      = std::nullopt,
         .floating      = std::nullopt,
         .pinned        = std::nullopt,
         .fullscreen    = std::nullopt,
         .urgent        = std::nullopt},
        {.address       = "0x2",
         .workspace     = {.id = 2, .name = std::nullopt},
         .class_name    = std::nullopt,
         .title         = std::nullopt,
         .initial_class = std::nullopt,
         .initial_title = std::nullopt,
         .app_id        = std::nullopt,
         .pid           = 9000,
         .command       = std::string("preset"),
         .geometry      = std::nullopt,
         .floating      = std::nullopt,
         .pinned        = std::nullopt,
         .fullscreen    = std::nullopt,
         .urgent        = std::nullopt},
    };

    hyprspaces::fill_client_commands(clients, proc_root);

    ASSERT_TRUE(clients[0].command.has_value());
    EXPECT_EQ(*clients[0].command, "kitty --class term");
    ASSERT_TRUE(clients[1].command.has_value());
    EXPECT_EQ(*clients[1].command, "preset");
}
