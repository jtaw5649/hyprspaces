#include "hyprspaces/autostart.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

#include "hyprspaces/strings.hpp"

namespace hyprspaces {

    namespace {

        std::string to_lower(std::string_view value) {
            std::string out;
            out.reserve(value.size());
            for (const char ch : value) {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
            return out;
        }

        std::vector<std::string> split_colon_paths(std::string_view value) {
            std::vector<std::string> parts;
            std::string              current;
            for (const char ch : value) {
                if (ch == ':') {
                    if (!current.empty()) {
                        parts.push_back(current);
                    }
                    current.clear();
                    continue;
                }
                current.push_back(ch);
            }
            if (!current.empty()) {
                parts.push_back(current);
            }
            return parts;
        }

    } // namespace

    bool autostart_hidden(std::string_view app_id, const std::vector<std::filesystem::path>& autostart_dirs) {
        if (app_id.empty()) {
            return false;
        }
        std::error_code             error;
        const std::filesystem::path filename = std::string(app_id) + ".desktop";
        for (const auto& dir : autostart_dirs) {
            const auto path = dir / filename;
            if (!std::filesystem::exists(path, error)) {
                continue;
            }
            std::ifstream input(path);
            if (!input) {
                continue;
            }
            std::string line;
            while (std::getline(input, line)) {
                const auto trimmed = hyprspaces::trim_copy(line);
                if (trimmed.rfind("Hidden=", 0) != 0) {
                    continue;
                }
                const auto value = hyprspaces::trim_copy(trimmed.substr(7));
                return to_lower(value) == "true";
            }
            return false;
        }
        return false;
    }

    std::vector<std::filesystem::path> autostart_paths_from_env() {
        std::vector<std::filesystem::path> paths;
        const char*                        config_home = std::getenv("XDG_CONFIG_HOME");
        const char*                        home        = std::getenv("HOME");
        if (config_home && *config_home) {
            paths.emplace_back(std::filesystem::path(config_home) / "autostart");
        } else if (home && *home) {
            paths.emplace_back(std::filesystem::path(home) / ".config" / "autostart");
        }

        const char* config_dirs = std::getenv("XDG_CONFIG_DIRS");
        if (config_dirs && *config_dirs) {
            for (const auto& entry : split_colon_paths(config_dirs)) {
                paths.emplace_back(std::filesystem::path(entry) / "autostart");
            }
        } else {
            paths.emplace_back(std::filesystem::path("/etc/xdg") / "autostart");
        }
        return paths;
    }

} // namespace hyprspaces
