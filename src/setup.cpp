#include "hyprspaces/setup.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace hyprspaces {

    namespace {

        constexpr std::string_view kWaybarWorkspacesJson =
            R"({"hyprland/workspaces":{"format":"{id}"}}
)";

        constexpr std::string_view kWaybarWorkspacesCss =
            R"(#workspaces {
  padding: 0 8px;
}
)";

        constexpr std::string_view kWaybarThemeCss =
            R"(@define-color foreground #c0caf5;
)";

        constexpr std::string_view kHyprspacesConf =
            R"(# hyprspaces.conf
# Source this file from your hyprland.conf:
# source = ~/.config/hyprspaces/hyprspaces.conf

# Plugin overrides (optional)
# plugin:hyprspaces:monitor_group = name:left-pair, monitors:DP-1;DP-2, workspace_offset:0
# plugin:hyprspaces:workspace_count = 10
# plugin:hyprspaces:wrap_cycling = true
# plugin:hyprspaces:safe_mode = true
# plugin:hyprspaces:debug_logging = false
# plugin:hyprspaces:waybar_theme_css = ~/.config/hyprspaces/waybar/theme.css
# plugin:hyprspaces:notify_session = true
# plugin:hyprspaces:notify_timeout_ms = 3000
# plugin:hyprspaces:session_enabled = false
# plugin:hyprspaces:session_reason = session_restore
# plugin:hyprspaces:session_autosave = false
# plugin:hyprspaces:session_autorestore = false

# Profiles (hyprlang)
# source = ~/.config/hypr/hyprspaces/profiles/*

# Workspace keybind takeover (Omarchy defaults). Remove if undesired.
unbind = SUPER, code:10
unbind = SUPER, code:11
unbind = SUPER, code:12
unbind = SUPER, code:13
unbind = SUPER, code:14
unbind = SUPER, code:15
unbind = SUPER, code:16
unbind = SUPER, code:17
unbind = SUPER, code:18
unbind = SUPER, code:19
unbind = SUPER SHIFT, code:10
unbind = SUPER SHIFT, code:11
unbind = SUPER SHIFT, code:12
unbind = SUPER SHIFT, code:13
unbind = SUPER SHIFT, code:14
unbind = SUPER SHIFT, code:15
unbind = SUPER SHIFT, code:16
unbind = SUPER SHIFT, code:17
unbind = SUPER SHIFT, code:18
unbind = SUPER SHIFT, code:19
unbind = SUPER SHIFT ALT, code:10
unbind = SUPER SHIFT ALT, code:11
unbind = SUPER SHIFT ALT, code:12
unbind = SUPER SHIFT ALT, code:13
unbind = SUPER SHIFT ALT, code:14
unbind = SUPER SHIFT ALT, code:15
unbind = SUPER SHIFT ALT, code:16
unbind = SUPER SHIFT ALT, code:17
unbind = SUPER SHIFT ALT, code:18
unbind = SUPER SHIFT ALT, code:19
unbind = SUPER, TAB
unbind = SUPER SHIFT, TAB
unbind = SUPER CTRL, TAB
unbind = SUPER, mouse_down
unbind = SUPER, mouse_up

bind = SUPER, code:10, hyprspaces:paired_switch, 1
bind = SUPER, code:11, hyprspaces:paired_switch, 2
bind = SUPER, code:12, hyprspaces:paired_switch, 3
bind = SUPER, code:13, hyprspaces:paired_switch, 4
bind = SUPER, code:14, hyprspaces:paired_switch, 5
bind = SUPER, code:15, hyprspaces:paired_switch, 6
bind = SUPER, code:16, hyprspaces:paired_switch, 7
bind = SUPER, code:17, hyprspaces:paired_switch, 8
bind = SUPER, code:18, hyprspaces:paired_switch, 9
bind = SUPER, code:19, hyprspaces:paired_switch, 10
bind = SUPER SHIFT, code:10, hyprspaces:paired_move_window, 1
bind = SUPER SHIFT, code:11, hyprspaces:paired_move_window, 2
bind = SUPER SHIFT, code:12, hyprspaces:paired_move_window, 3
bind = SUPER SHIFT, code:13, hyprspaces:paired_move_window, 4
bind = SUPER SHIFT, code:14, hyprspaces:paired_move_window, 5
bind = SUPER SHIFT, code:15, hyprspaces:paired_move_window, 6
bind = SUPER SHIFT, code:16, hyprspaces:paired_move_window, 7
bind = SUPER SHIFT, code:17, hyprspaces:paired_move_window, 8
bind = SUPER SHIFT, code:18, hyprspaces:paired_move_window, 9
bind = SUPER SHIFT, code:19, hyprspaces:paired_move_window, 10
bind = SUPER SHIFT ALT, code:10, hyprspaces:paired_move_window, 1
bind = SUPER SHIFT ALT, code:11, hyprspaces:paired_move_window, 2
bind = SUPER SHIFT ALT, code:12, hyprspaces:paired_move_window, 3
bind = SUPER SHIFT ALT, code:13, hyprspaces:paired_move_window, 4
bind = SUPER SHIFT ALT, code:14, hyprspaces:paired_move_window, 5
bind = SUPER SHIFT ALT, code:15, hyprspaces:paired_move_window, 6
bind = SUPER SHIFT ALT, code:16, hyprspaces:paired_move_window, 7
bind = SUPER SHIFT ALT, code:17, hyprspaces:paired_move_window, 8
bind = SUPER SHIFT ALT, code:18, hyprspaces:paired_move_window, 9
bind = SUPER SHIFT ALT, code:19, hyprspaces:paired_move_window, 10
bind = SUPER, TAB, hyprspaces:paired_cycle, next
bind = SUPER SHIFT, TAB, hyprspaces:paired_cycle, prev
bind = SUPER CTRL, TAB, hyprspaces:paired_cycle, prev
bind = SUPER, mouse_down, hyprspaces:paired_cycle, next
bind = SUPER, mouse_up, hyprspaces:paired_cycle, prev

# Keybinds (examples)
# bind = SUPER CTRL ALT, S, hyprspaces:session_save
# bind = SUPER CTRL ALT, R, hyprspaces:session_restore
# bind = SUPER CTRL ALT, RIGHT, hyprspaces:paired_cycle, next
# bind = SUPER CTRL ALT, LEFT, hyprspaces:paired_cycle, prev
# bind = SUPER CTRL ALT, 1, hyprspaces:paired_switch, 1
# bind = SUPER CTRL ALT, 2, hyprspaces:paired_switch, 2
)";

        std::optional<std::string> write_file(const std::filesystem::path& path, std::string_view contents) {
            std::ofstream output(path);
            if (!output.good()) {
                return "unable to write " + path.string();
            }
            output << contents;
            output.flush();
            if (!output.good()) {
                return "unable to write " + path.string();
            }
            return std::nullopt;
        }

        std::string read_file_contents(const std::filesystem::path& path) {
            std::ifstream      input(path);
            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }

        std::string_view ltrim(std::string_view value) {
            size_t offset = 0;
            while (offset < value.size() && std::isspace(static_cast<unsigned char>(value[offset]))) {
                ++offset;
            }
            return value.substr(offset);
        }

        bool contains_hyprspaces_source(const std::string& contents) {
            std::istringstream input(contents);
            std::string        line;
            while (std::getline(input, line)) {
                const auto trimmed = ltrim(line);
                if (trimmed.empty() || trimmed.front() == '#') {
                    continue;
                }
                if (!trimmed.starts_with("source")) {
                    continue;
                }
                if (trimmed.find("hyprspaces.conf") != std::string_view::npos) {
                    return true;
                }
            }
            return false;
        }

    } // namespace

    std::string render_hyprspaces_conf() {
        return std::string(kHyprspacesConf);
    }

    bool ensure_hyprspaces_conf(const std::filesystem::path& config_path) {
        if (std::filesystem::exists(config_path)) {
            return false;
        }

        if (const auto parent = config_path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream output(config_path);
        if (!output.good()) {
            return false;
        }
        output << render_hyprspaces_conf();
        output.flush();
        if (!output.good()) {
            return false;
        }
        return true;
    }

    bool ensure_hyprland_conf_source(const std::filesystem::path& hyprland_conf_path, const std::filesystem::path& include_path) {
        std::string contents;
        if (std::filesystem::exists(hyprland_conf_path)) {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(hyprland_conf_path, ec) || ec) {
                return false;
            }
            contents = read_file_contents(hyprland_conf_path);
            if (contains_hyprspaces_source(contents)) {
                return false;
            }
        }

        if (const auto parent = hyprland_conf_path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream output(hyprland_conf_path, std::ios::app);
        if (!output.good()) {
            return false;
        }
        if (!contents.empty() && contents.back() != '\n') {
            output << '\n';
        }
        output << "source = " << include_path.string() << '\n';
        output.flush();
        if (!output.good()) {
            return false;
        }
        return true;
    }

    bool ensure_hyprspaces_bootstrap(const Paths& paths) {
        bool changed = false;
        if (ensure_hyprspaces_conf(paths.hyprspaces_conf_path)) {
            changed = true;
        }
        std::error_code ec;
        if (std::filesystem::create_directories(paths.profiles_dir, ec)) {
            changed = true;
        }
        if (ensure_hyprland_conf_source(paths.hyprland_conf_path, paths.hyprspaces_conf_path)) {
            changed = true;
        }
        return changed;
    }

    std::vector<WorkspaceRuleSpec> workspace_rules_for_config(const Config& config) {
        std::vector<WorkspaceRuleSpec> rules;
        if (config.workspace_count <= 0) {
            return rules;
        }
        for (const auto& group : config.monitor_groups) {
            for (int index = 0; index < static_cast<int>(group.monitors.size()); ++index) {
                for (int slot = 1; slot <= config.workspace_count; ++slot) {
                    const auto workspace_id = workspace_id_for_group(group, config.workspace_count, index, slot);
                    if (!workspace_id) {
                        continue;
                    }
                    rules.push_back(WorkspaceRuleSpec{
                        .workspace_id = *workspace_id,
                        .monitor      = group.monitors[static_cast<size_t>(index)],
                        .is_default   = slot == 1,
                    });
                }
            }
        }
        return rules;
    }

    std::vector<std::string> workspace_rule_lines_for_config(const Config& config) {
        const auto               specs = workspace_rules_for_config(config);
        std::vector<std::string> lines;
        lines.reserve(specs.size());

        for (const auto& spec : specs) {
            std::string line = std::to_string(spec.workspace_id);
            line += ", monitor:";
            line += spec.monitor;
            if (spec.is_default) {
                line += ", default:true";
            }
            if (config.persistent_all ||
                std::find(config.persistent_workspaces.begin(), config.persistent_workspaces.end(), spec.workspace_id) != config.persistent_workspaces.end()) {
                line += ", persistent:true";
            }
            line += ", layoutopt:hyprspaces:1";
            lines.push_back(std::move(line));
        }

        return lines;
    }

    std::optional<std::string> install_waybar_assets(const Paths& paths, const std::optional<std::string>& theme_override) {
        try {
            std::filesystem::create_directories(paths.waybar_dir);

            if (const auto error = write_file(paths.waybar_dir / "workspaces.json", kWaybarWorkspacesJson)) {
                return error;
            }
            if (const auto error = write_file(paths.waybar_dir / "workspaces.css", kWaybarWorkspacesCss)) {
                return error;
            }

            std::string theme_contents;
            if (theme_override) {
                std::ifstream input(*theme_override);
                if (!input.good()) {
                    return "unable to read theme css override";
                }
                theme_contents.assign((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            } else {
                theme_contents.assign(kWaybarThemeCss.begin(), kWaybarThemeCss.end());
            }
            if (const auto error = write_file(paths.waybar_theme_css, theme_contents)) {
                return error;
            }

        } catch (const std::exception& ex) { return ex.what(); }

        return std::nullopt;
    }

    std::optional<std::string> uninstall_waybar_assets(const Paths& paths) {
        std::error_code ec;
        std::filesystem::remove_all(paths.waybar_dir, ec);
        if (ec) {
            return "unable to remove waybar assets";
        }
        return std::nullopt;
    }

} // namespace hyprspaces
