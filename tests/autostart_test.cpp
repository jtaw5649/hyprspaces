#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "hyprspaces/autostart.hpp"

namespace {

    std::filesystem::path make_autostart_dir(const std::string& name) {
        const auto dir = std::filesystem::temp_directory_path() / name / "autostart";
        std::filesystem::create_directories(dir);
        return dir;
    }

    void write_desktop_entry(const std::filesystem::path& path, std::string_view contents) {
        std::ofstream output(path);
        output << contents;
    }

} // namespace

TEST(Autostart, HiddenDesktopEntrySkipsRelaunch) {
    const auto dir   = make_autostart_dir("hyprspaces-autostart-hidden");
    const auto entry = dir / "org.test.App.desktop";
    write_desktop_entry(entry, "[Desktop Entry]\nHidden=true\n");

    EXPECT_TRUE(hyprspaces::autostart_hidden("org.test.App", {dir}));

    std::filesystem::remove_all(dir.parent_path());
}

TEST(Autostart, MissingEntryIsNotHidden) {
    const auto dir = make_autostart_dir("hyprspaces-autostart-missing");

    EXPECT_FALSE(hyprspaces::autostart_hidden("org.test.App", {dir}));

    std::filesystem::remove_all(dir.parent_path());
}

TEST(Autostart, HiddenFalseDoesNotSkip) {
    const auto dir   = make_autostart_dir("hyprspaces-autostart-visible");
    const auto entry = dir / "org.test.App.desktop";
    write_desktop_entry(entry, "[Desktop Entry]\nHidden=false\n");

    EXPECT_FALSE(hyprspaces::autostart_hidden("org.test.App", {dir}));

    std::filesystem::remove_all(dir.parent_path());
}
