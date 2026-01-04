#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>

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

    std::filesystem::path cleanup_script_path() {
        return std::filesystem::path(__FILE__).parent_path() / ".." / "scripts" / "cleanup-headless.sh";
    }

    std::filesystem::path test_env_script_path() {
        return std::filesystem::path(__FILE__).parent_path() / ".." / "scripts" / "test-env.sh";
    }

    std::filesystem::path make_temp_dir(const std::string& prefix) {
        const auto now  = std::chrono::steady_clock::now().time_since_epoch().count();
        auto       path = std::filesystem::temp_directory_path() / (prefix + "-" + std::to_string(now));
        std::filesystem::create_directories(path);
        return path;
    }

    void write_executable(const std::filesystem::path& path, const std::string& content) {
        std::ofstream out(path);
        out << content;
        out.close();

        auto perms = std::filesystem::status(path).permissions();
        std::filesystem::permissions(path, perms | std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec);
    }

    std::string read_file(const std::filesystem::path& path) {
        std::ifstream      in(path);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    bool jq_available() {
        return std::system("command -v jq >/dev/null 2>&1") == 0;
    }

    std::string hyprctl_stub_script() {
        return R"(#!/usr/bin/env bash
set -euo pipefail

log_file="${HYPRSPACES_TEST_LOG:-}"
cmd="${1:-}"
shift || true

case "$cmd" in
  monitors)
    if [[ "${1:-}" == "-j" ]]; then
      cat <<'JSON'
[
  {"name":"HEADLESS-2","width":1920,"height":1080},
  {"name":"eDP-1","width":1920,"height":1080},
  {"name":"HEADLESS-3","width":1920,"height":1080}
]
JSON
      exit 0
    fi
    ;;
  workspaces)
    if [[ "${1:-}" == "-j" ]]; then
      echo "[]"
      exit 0
    fi
    ;;
  dispatch)
    echo "ok"
    exit 0
    ;;
  output)
    if [[ "${1:-}" == "remove" ]]; then
      echo "${2:-}" >> "$log_file"
      exit 0
    fi
    ;;
esac

echo "unexpected hyprctl args: $cmd $*" >&2
exit 2
)";
    }

} // namespace

TEST(HeadlessCleanupScript, RemovesHeadlessOutputs) {
    if (!jq_available()) {
        GTEST_SKIP() << "jq is required for cleanup script tests";
    }

    const auto script = cleanup_script_path();
    ASSERT_TRUE(std::filesystem::exists(script)) << "Missing script: " << script.string();

    const auto temp_root = make_temp_dir("hyprspaces-cleanup");
    const auto fake_bin  = temp_root / "bin";
    const auto log_path  = temp_root / "removed.log";
    std::filesystem::create_directories(fake_bin);

    write_executable(fake_bin / "hyprctl", hyprctl_stub_script());

    const std::string command = "env PATH=\"" + fake_bin.string() + ":$PATH\" HYPRSPACES_TEST_LOG=\"" + log_path.string() + "\" bash \"" + script.string() + "\"";

    const auto        result = run_command(command);
    ASSERT_EQ(result.exit_status, 0) << result.output;

    const auto log = read_file(log_path);
    EXPECT_NE(log.find("HEADLESS-2"), std::string::npos);
    EXPECT_NE(log.find("HEADLESS-3"), std::string::npos);
    EXPECT_EQ(log.find("eDP-1"), std::string::npos);
}

TEST(TestEnvScript, RunTestsCleansHeadlessOutputs) {
    if (!jq_available()) {
        GTEST_SKIP() << "jq is required for test-env script tests";
    }

    const auto script = test_env_script_path();
    ASSERT_TRUE(std::filesystem::exists(script)) << "Missing script: " << script.string();

    const auto temp_root   = make_temp_dir("hyprspaces-test-env");
    const auto fake_bin    = temp_root / "bin";
    const auto log_path    = temp_root / "removed.log";
    const auto runtime_dir = temp_root / "runtime";
    std::filesystem::create_directories(fake_bin);
    std::filesystem::create_directories(runtime_dir / "hyprspaces-dev");
    std::filesystem::create_directories(runtime_dir / "hypr" / "TEST-SIG");

    write_executable(fake_bin / "hyprctl", hyprctl_stub_script());

    std::ofstream(runtime_dir / "hyprspaces-dev" / "nested.sig") << "TEST-SIG";

    const std::string command = "env XDG_RUNTIME_DIR=\"" + runtime_dir.string() + "\" PATH=\"" + fake_bin.string() + ":$PATH\" HYPRSPACES_TEST_LOG=\"" + log_path.string() +
        "\" bash \"" + script.string() + "\" test 0";

    const auto result = run_command(command);
    ASSERT_EQ(result.exit_status, 0) << result.output;

    const auto log = read_file(log_path);
    EXPECT_NE(log.find("HEADLESS-2"), std::string::npos);
    EXPECT_NE(log.find("HEADLESS-3"), std::string::npos);
}
