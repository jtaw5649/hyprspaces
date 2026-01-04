#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/monitor_groups.hpp"
#include "hyprspaces/types.hpp"

namespace hyprspaces {

    enum class MonitorSelectorKind {
        kName,
        kDescription,
    };

    struct MonitorSelector {
        MonitorSelectorKind kind = MonitorSelectorKind::kName;
        std::string         value;
    };

    struct MonitorProfile {
        std::string                  name;
        std::vector<MonitorSelector> selectors;
    };

    struct MonitorGroupSelection {
        std::optional<std::string> active_profile;
        std::vector<MonitorGroup>  groups;
        bool                       used_fallback = false;
    };

    MonitorProfile        parse_monitor_profile(std::string_view value);
    MonitorGroupSelection select_monitor_groups(const std::vector<MonitorGroup>& groups, const std::vector<MonitorProfile>& profiles,
                                                const std::optional<std::vector<MonitorInfo>>& monitors);

} // namespace hyprspaces
