#include "hyprspaces/waybar.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "hyprspaces/pairing.hpp"
#include "hyprspaces/strings.hpp"

namespace hyprspaces {

    namespace {

        constexpr double           kMidFactor       = 0.65;
        constexpr double           kDimFactor       = 0.40;
        constexpr std::string_view kGlyph           = "\U000F14FB";
        constexpr std::string_view kDefaultThemeCss = R"(@define-color foreground #c0caf5;
)";

        bool                       is_special_workspace(const std::optional<std::string>& name) {
            return name && name->starts_with("special:");
        }

        bool is_hex_digit(char ch) {
            return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
        }

        std::optional<std::string> extract_foreground_hex(std::string_view css) {
            const std::string_view key = "@define-color foreground";
            const auto             pos = css.find(key);
            if (pos == std::string_view::npos) {
                return std::nullopt;
            }
            const auto hash = css.find('#', pos + key.size());
            if (hash == std::string_view::npos || hash + 7 > css.size()) {
                return std::nullopt;
            }
            const std::string_view hex = css.substr(hash, 7);
            if (hex.size() != 7) {
                return std::nullopt;
            }
            for (size_t i = 1; i < hex.size(); ++i) {
                if (!is_hex_digit(hex[i])) {
                    return std::nullopt;
                }
            }
            std::string normalized(hex);
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) { return std::tolower(ch); });
            return normalized;
        }

        std::optional<int> parse_hex_byte(std::string_view value) {
            int result = 0;
            for (const char ch : value) {
                if (!is_hex_digit(ch)) {
                    return std::nullopt;
                }
                result *= 16;
                if (ch >= '0' && ch <= '9') {
                    result += ch - '0';
                } else if (ch >= 'a' && ch <= 'f') {
                    result += 10 + (ch - 'a');
                } else if (ch >= 'A' && ch <= 'F') {
                    result += 10 + (ch - 'A');
                }
            }
            return result;
        }

        struct Rgb {
            int r = 0;
            int g = 0;
            int b = 0;
        };

        struct WaybarSlotState {
            int  windows = 0;
            bool urgent  = false;
            bool visible = false;
        };

        struct WaybarRenderState {
            std::string                  label;
            std::string                  group_class;
            int                          active_slot = 0;
            std::vector<WaybarSlotState> slots;
        };

        std::optional<Rgb> parse_rgb(std::string_view hex) {
            if (hex.size() != 7 || hex[0] != '#') {
                return std::nullopt;
            }
            auto r = parse_hex_byte(hex.substr(1, 2));
            auto g = parse_hex_byte(hex.substr(3, 2));
            auto b = parse_hex_byte(hex.substr(5, 2));
            if (!r || !g || !b) {
                return std::nullopt;
            }
            return Rgb{*r, *g, *b};
        }

        int scale_channel(int value, double factor) {
            const auto scaled = static_cast<int>(value * factor);
            return std::clamp(scaled, 0, 255);
        }

        Rgb scale_rgb(const Rgb& color, double factor) {
            return Rgb{scale_channel(color.r, factor), scale_channel(color.g, factor), scale_channel(color.b, factor)};
        }

        std::string to_hex(const Rgb& color) {
            std::ostringstream out;
            out << '#';
            out << std::hex;
            out.width(2);
            out.fill('0');
            out << std::nouppercase << color.r;
            out.width(2);
            out.fill('0');
            out << std::nouppercase << color.g;
            out.width(2);
            out.fill('0');
            out << std::nouppercase << color.b;
            return out.str();
        }

        std::string sanitize_class_token(std::string_view value) {
            std::string output;
            output.reserve(value.size());
            bool pending_dash = false;
            for (const char ch : value) {
                if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
                    if (pending_dash && !output.empty()) {
                        output.push_back('-');
                    }
                    pending_dash = false;
                    output.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                } else if (ch == '-' || ch == '_') {
                    if (pending_dash && !output.empty()) {
                        output.push_back('-');
                        pending_dash = false;
                    }
                    output.push_back(ch);
                } else {
                    pending_dash = true;
                }
            }
            while (!output.empty() && output.back() == '-') {
                output.pop_back();
            }
            while (!output.empty() && output.front() == '-') {
                output.erase(output.begin());
            }
            return output;
        }

        void append_class(std::string& classes, std::string_view token) {
            if (token.empty()) {
                return;
            }
            if (!classes.empty()) {
                classes.push_back(' ');
            }
            classes.append(token);
        }

        std::optional<int> slot_for_workspace(int workspace_id, int slot_count, const MonitorGroup* group, const std::vector<MonitorGroup>& groups) {
            if (workspace_id <= 0 || slot_count <= 0) {
                return std::nullopt;
            }
            if (group) {
                const auto match = find_group_for_workspace(workspace_id, slot_count, groups);
                if (!match || match->group != group) {
                    return std::nullopt;
                }
                return match->normalized_workspace;
            }
            return normalize_workspace(workspace_id, slot_count);
        }

        WaybarRenderState build_waybar_state(const Config& config, int active_workspace_id, const std::vector<WorkspaceInfo>& workspaces, const std::vector<MonitorInfo>& monitors,
                                             const std::vector<ClientInfo>& clients) {
            WaybarRenderState state;
            const int         slot_count = config.workspace_count;
            state.slots.resize(static_cast<size_t>(std::max(slot_count, 0)));

            const MonitorGroup* active_group = nullptr;
            if (!config.monitor_groups.empty()) {
                const auto active_match = find_group_for_workspace(active_workspace_id, slot_count, config.monitor_groups);
                if (active_match) {
                    active_group      = active_match->group;
                    state.active_slot = active_match->normalized_workspace;
                }
            }

            if (!active_group && active_workspace_id > 0 && slot_count > 0) {
                state.active_slot = normalize_workspace(active_workspace_id, slot_count);
            }

            if (active_group) {
                state.label       = trim_copy(active_group->name);
                state.group_class = sanitize_class_token(state.label);
            }

            for (const auto& workspace : workspaces) {
                if (workspace.windows <= 0) {
                    continue;
                }
                if (is_special_workspace(workspace.name)) {
                    continue;
                }
                const auto slot = slot_for_workspace(workspace.id, slot_count, active_group, config.monitor_groups);
                if (!slot || *slot <= 0 || *slot > slot_count) {
                    continue;
                }
                state.slots[static_cast<size_t>(*slot - 1)].windows += workspace.windows;
            }

            for (const auto& client : clients) {
                if (!client.urgent || !*client.urgent) {
                    continue;
                }
                const auto slot = slot_for_workspace(client.workspace.id, slot_count, active_group, config.monitor_groups);
                if (!slot || *slot <= 0 || *slot > slot_count) {
                    continue;
                }
                state.slots[static_cast<size_t>(*slot - 1)].urgent = true;
            }

            for (const auto& monitor : monitors) {
                if (!monitor.active_workspace_id) {
                    continue;
                }
                if (active_workspace_id > 0 && *monitor.active_workspace_id == active_workspace_id) {
                    continue;
                }
                const auto slot = slot_for_workspace(*monitor.active_workspace_id, slot_count, active_group, config.monitor_groups);
                if (!slot || *slot <= 0 || *slot > slot_count) {
                    continue;
                }
                state.slots[static_cast<size_t>(*slot - 1)].visible = true;
            }

            return state;
        }

        std::string render_waybar_text(const WaybarRenderState& state, const WaybarTheme& theme) {
            const int          slot_count = static_cast<int>(state.slots.size());
            std::ostringstream output;
            if (!state.label.empty()) {
                output << state.label << ": ";
            }
            for (int slot = 1; slot <= slot_count; ++slot) {
                if (slot > 1) {
                    output << ' ';
                }
                const auto&        slot_state = state.slots[static_cast<size_t>(slot - 1)];
                const bool         is_active  = state.active_slot > 0 && slot == state.active_slot;
                const bool         is_visible = slot_state.visible && !is_active;
                const bool         is_urgent  = slot_state.urgent;
                const std::string& color      = (is_active || is_urgent) ? theme.foreground : (slot_state.windows > 0 ? theme.mid : theme.dim);
                output << "<span foreground='" << color << "'";
                if (is_urgent) {
                    output << " weight='bold'";
                }
                if (is_visible) {
                    output << " underline='single'";
                }
                output << ">";
                if (slot_state.urgent) {
                    output << '!';
                }
                if (is_active) {
                    output << kGlyph;
                } else {
                    output << slot;
                }
                if (slot_state.windows > 0) {
                    output << ":" << slot_state.windows;
                }
                output << "</span>";
            }
            return output.str();
        }

        std::string render_waybar_tooltip(const WaybarRenderState& state) {
            const int          slot_count = static_cast<int>(state.slots.size());
            std::ostringstream output;
            if (!state.label.empty()) {
                output << state.label << ": ";
            }
            for (int slot = 1; slot <= slot_count; ++slot) {
                if (slot > 1) {
                    output << ", ";
                }
                const auto& slot_state = state.slots[static_cast<size_t>(slot - 1)];
                output << slot;
                if (slot_state.windows > 0) {
                    output << "(" << slot_state.windows << ")";
                }
                const bool is_active = state.active_slot > 0 && slot == state.active_slot;
                if (slot_state.urgent || is_active || slot_state.visible) {
                    output << ' ';
                }
                bool needs_space = false;
                if (slot_state.urgent) {
                    output << "urgent";
                    needs_space = true;
                }
                if (is_active) {
                    if (needs_space) {
                        output << ' ';
                    }
                    output << "active";
                    needs_space = true;
                } else if (slot_state.visible) {
                    if (needs_space) {
                        output << ' ';
                    }
                    output << "visible";
                }
            }
            return output.str();
        }

    } // namespace

    std::optional<WaybarTheme> parse_waybar_theme(std::string_view css) {
        const auto foreground = extract_foreground_hex(css);
        if (!foreground) {
            return std::nullopt;
        }
        const auto rgb = parse_rgb(*foreground);
        if (!rgb) {
            return std::nullopt;
        }
        return WaybarTheme{*foreground, to_hex(scale_rgb(*rgb, kMidFactor)), to_hex(scale_rgb(*rgb, kDimFactor))};
    }

    std::optional<WaybarTheme> WaybarThemeCache::load(const std::filesystem::path& path, std::string* error) {
        std::error_code exists_ec;
        const bool      exists = std::filesystem::exists(path, exists_ec);
        if (!exists || exists_ec) {
            const auto fallback = parse_waybar_theme(kDefaultThemeCss);
            if (!fallback) {
                if (error) {
                    *error = "missing waybar theme css";
                }
                return std::nullopt;
            }
            if (error) {
                error->clear();
            }
            cached_path_  = path;
            cached_theme_ = fallback;
            cached_mtime_.reset();
            return cached_theme_;
        }

        std::error_code ec;
        const auto      current_mtime = std::filesystem::last_write_time(path, ec);
        const bool      should_reload = cached_path_ != path || !cached_theme_ || ec || !cached_mtime_ || current_mtime != *cached_mtime_;
        if (!should_reload) {
            return cached_theme_;
        }

        std::ifstream input(path);
        if (!input.good()) {
            if (error) {
                *error = "missing waybar theme css";
            }
            return std::nullopt;
        }
        const std::string css((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (css.empty()) {
            if (error) {
                *error = "empty waybar theme css";
            }
            return std::nullopt;
        }
        const auto parsed = parse_waybar_theme(css);
        if (!parsed) {
            if (error) {
                *error = "invalid waybar theme css";
            }
            return std::nullopt;
        }

        cached_path_  = path;
        cached_theme_ = parsed;
        if (error) {
            error->clear();
        }
        if (ec) {
            std::error_code mtime_ec;
            const auto      updated_mtime = std::filesystem::last_write_time(path, mtime_ec);
            if (mtime_ec) {
                cached_mtime_.reset();
            } else {
                cached_mtime_ = updated_mtime;
            }
        } else {
            cached_mtime_ = current_mtime;
        }
        return cached_theme_;
    }

    void WaybarThemeCache::invalidate() {
        cached_path_.clear();
        cached_mtime_.reset();
        cached_theme_.reset();
    }

    std::string render_waybar_json(const Config& config, int active_workspace_id, const std::vector<WorkspaceInfo>& workspaces, const std::vector<MonitorInfo>& monitors,
                                   const std::vector<ClientInfo>& clients, const WaybarTheme& theme) {
        const auto state   = build_waybar_state(config, active_workspace_id, workspaces, monitors, clients);
        const auto text    = render_waybar_text(state, theme);
        const auto tooltip = render_waybar_tooltip(state);

        int        occupied_slots = 0;
        bool       has_urgent     = false;
        bool       has_visible    = false;
        for (const auto& slot_state : state.slots) {
            if (slot_state.windows > 0) {
                ++occupied_slots;
            }
            if (slot_state.urgent) {
                has_urgent = true;
            }
            if (slot_state.visible) {
                has_visible = true;
            }
        }
        const int   slot_count = static_cast<int>(state.slots.size());
        const int   percentage = slot_count > 0 ? (occupied_slots * 100) / slot_count : 0;

        std::string classes = "workspaces";
        if (!state.group_class.empty()) {
            append_class(classes, std::string("group-") + state.group_class);
            append_class(classes, std::string("group-active-") + state.group_class);
        }
        if (has_urgent) {
            append_class(classes, "has-urgent");
        }
        if (has_visible) {
            append_class(classes, "has-visible");
        }
        if (config.waybar_class) {
            const auto trimmed = trim_copy(*config.waybar_class);
            if (!trimmed.empty()) {
                append_class(classes, trimmed);
            }
        }

        const auto  text_json    = nlohmann::json(text).dump(-1, ' ', true);
        const auto  tooltip_json = nlohmann::json(tooltip).dump(-1, ' ', true);
        const auto  class_json   = nlohmann::json(classes).dump(-1, ' ', true);
        std::string output;
        output.reserve(text_json.size() + tooltip_json.size() + class_json.size() + 96);
        output.append("{\"text\": ");
        output.append(text_json);
        output.append(", \"tooltip\": ");
        output.append(tooltip_json);
        output.append(", \"class\": ");
        output.append(class_json);
        output.append(", \"percentage\": ");
        output.append(std::to_string(percentage));
        output.append(", \"markup\": true}");
        output.push_back('\n');
        return output;
    }

} // namespace hyprspaces
