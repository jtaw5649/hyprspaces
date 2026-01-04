#include "hyprspaces/status.hpp"

#include <sstream>

#include <nlohmann/json.hpp>

#include "hyprspaces/pairing.hpp"

namespace hyprspaces {

    namespace {

        std::string waybar_theme_label(const Config& config) {
            if (config.waybar_theme_css) {
                return *config.waybar_theme_css;
            }
            return "default";
        }

    } // namespace

    std::string render_status(const Config& config, int active_workspace_id) {
        std::ostringstream output;
        output << "Hyprspaces: loaded\n";
        output << "Rules: runtime\n";
        output << "Wrap cycling: " << (config.wrap_cycling ? "enabled" : "disabled") << "\n";
        output << "Waybar theme: " << waybar_theme_label(config) << "\n\n";
        output << "Monitor groups:\n";
        if (config.monitor_groups.empty()) {
            output << "  (none)\n\n";
        } else {
            for (const auto& group : config.monitor_groups) {
                output << "  " << group.name << " (offset " << group.workspace_offset << "): ";
                for (size_t i = 0; i < group.monitors.size(); ++i) {
                    if (i != 0) {
                        output << ", ";
                    }
                    output << group.monitors[i];
                }
                output << "\n";
            }
            output << "\n";
        }

        const auto match = find_group_for_workspace(active_workspace_id, config.workspace_count, config.monitor_groups);
        if (match && match->group) {
            output << "Active group: " << match->group->name << "\n";
            output << "Active workspace: " << active_workspace_id << " (normalized " << match->normalized_workspace << ")";
        } else {
            output << "Active group: (none)\n";
            output << "Active workspace: " << active_workspace_id;
        }

        output << "\n";
        return output.str();
    }

    std::string render_status_json(const Config& config, int active_workspace_id) {
        nlohmann::json groups = nlohmann::json::array();
        for (const auto& group : config.monitor_groups) {
            nlohmann::json entry;
            entry["name"]             = group.name;
            entry["workspace_offset"] = group.workspace_offset;
            entry["monitors"]         = group.monitors;
            groups.push_back(std::move(entry));
        }

        nlohmann::json active_group = nullptr;
        if (const auto match = find_group_for_workspace(active_workspace_id, config.workspace_count, config.monitor_groups); match && match->group) {
            active_group = nlohmann::json{
                {"name", match->group->name},
                {"workspace_id", active_workspace_id},
                {"normalized", match->normalized_workspace},
                {"monitor_index", match->monitor_index},
            };
        }

        nlohmann::json json{
            {"status", "loaded"},
            {"rules", "runtime"},
            {"wrap_cycling", config.wrap_cycling},
            {"waybar_theme", waybar_theme_label(config)},
            {"workspace_count", config.workspace_count},
            {"monitor_groups", groups},
            {"active_group", active_group},
        };
        return json.dump();
    }

} // namespace hyprspaces
