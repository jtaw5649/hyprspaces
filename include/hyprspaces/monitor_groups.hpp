#ifndef HYPRSPACES_MONITOR_GROUPS_HPP
#define HYPRSPACES_MONITOR_GROUPS_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hyprspaces {

    struct MonitorGroup {
        std::optional<std::string> profile;
        std::string                name;
        std::vector<std::string>   monitors;
        int                        workspace_offset = 0;
    };

    struct MonitorGroupMatch {
        const MonitorGroup* group                = nullptr;
        int                 monitor_index        = -1;
        int                 normalized_workspace = 0;
    };

    MonitorGroup                     parse_monitor_group(std::string_view value);
    std::optional<MonitorGroupMatch> find_group_for_monitor(std::string_view monitor, const std::vector<MonitorGroup>& groups);
    std::optional<MonitorGroupMatch> find_group_for_workspace(int workspace_id, int workspace_count, const std::vector<MonitorGroup>& groups);
    std::optional<int>               workspace_id_for_group(const MonitorGroup& group, int workspace_count, int monitor_index, int normalized_workspace);

} // namespace hyprspaces

#endif // HYPRSPACES_MONITOR_GROUPS_HPP
