#include "hyprspaces/monitor_profiles.hpp"

#include <stdexcept>

#include "hyprspaces/strings.hpp"

namespace hyprspaces {

    namespace {

        std::vector<std::string> split_tokens(std::string_view value, char delim) {
            std::vector<std::string> tokens;
            size_t                   start = 0;
            while (start <= value.size()) {
                const auto end  = value.find(delim, start);
                const auto part = end == std::string_view::npos ? value.substr(start) : value.substr(start, end - start);
                tokens.emplace_back(trim_copy(part));
                if (end == std::string_view::npos) {
                    break;
                }
                start = end + 1;
            }
            return tokens;
        }

        std::pair<std::string, std::string> split_key_value(std::string_view token) {
            const auto colon = token.find(':');
            if (colon == std::string_view::npos) {
                throw std::runtime_error("monitor profile entry missing ':'");
            }
            const auto key   = trim_copy(token.substr(0, colon));
            const auto value = trim_copy(token.substr(colon + 1));
            if (key.empty() || value.empty()) {
                throw std::runtime_error("monitor profile entry missing key/value");
            }
            return {key, value};
        }

        MonitorSelector parse_selector(std::string_view raw) {
            const auto token = trim_copy(raw);
            if (token.empty()) {
                throw std::runtime_error("monitor selector is empty");
            }
            if (token.starts_with("name:")) {
                return MonitorSelector{.kind = MonitorSelectorKind::kName, .value = std::string(token.substr(5))};
            }
            if (token.starts_with("desc:")) {
                return MonitorSelector{.kind = MonitorSelectorKind::kDescription, .value = std::string(token.substr(5))};
            }
            if (token.starts_with("description:")) {
                return MonitorSelector{.kind = MonitorSelectorKind::kDescription, .value = std::string(token.substr(12))};
            }
            return MonitorSelector{.kind = MonitorSelectorKind::kName, .value = std::string(token)};
        }

        std::vector<MonitorGroup> collect_default_groups(const std::vector<MonitorGroup>& groups) {
            std::vector<MonitorGroup> selected;
            for (const auto& group : groups) {
                if (!group.profile || group.profile->empty()) {
                    selected.push_back(group);
                }
            }
            return selected;
        }

        bool selector_matches(const MonitorSelector& selector, const MonitorInfo& monitor) {
            if (selector.kind == MonitorSelectorKind::kName) {
                return monitor.name == selector.value;
            }
            if (!monitor.description) {
                return false;
            }
            return *monitor.description == selector.value;
        }

        bool matches_profile(const MonitorProfile& profile, const std::vector<MonitorInfo>& monitors) {
            if (profile.selectors.empty()) {
                return false;
            }
            if (monitors.size() != profile.selectors.size()) {
                return false;
            }
            std::vector<bool> used(monitors.size(), false);
            for (const auto& selector : profile.selectors) {
                bool matched = false;
                for (size_t i = 0; i < monitors.size(); ++i) {
                    if (used[i]) {
                        continue;
                    }
                    if (selector_matches(selector, monitors[i])) {
                        used[i] = true;
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    return false;
                }
            }
            return true;
        }

    } // namespace

    MonitorProfile parse_monitor_profile(std::string_view value) {
        MonitorProfile profile;
        const auto     tokens = split_tokens(value, ',');
        for (const auto& raw : tokens) {
            if (raw.empty()) {
                throw std::runtime_error("monitor profile entry is empty");
            }
            const auto [key, val] = split_key_value(raw);
            if (key == "name") {
                profile.name = val;
                continue;
            }
            if (key == "monitors") {
                const auto selectors = split_tokens(val, ';');
                for (const auto& selector : selectors) {
                    profile.selectors.push_back(parse_selector(selector));
                }
                continue;
            }
            throw std::runtime_error("unknown monitor profile field");
        }

        if (profile.name.empty()) {
            throw std::runtime_error("monitor profile name is required");
        }
        if (profile.selectors.empty()) {
            throw std::runtime_error("monitor profile selectors are required");
        }
        return profile;
    }

    MonitorGroupSelection select_monitor_groups(const std::vector<MonitorGroup>& groups, const std::vector<MonitorProfile>& profiles,
                                                const std::optional<std::vector<MonitorInfo>>& monitors) {
        MonitorGroupSelection selection;
        if (!monitors || profiles.empty()) {
            selection.groups = collect_default_groups(groups);
            return selection;
        }

        const MonitorProfile* matched = nullptr;
        for (const auto& profile : profiles) {
            if (matches_profile(profile, *monitors)) {
                matched = &profile;
                break;
            }
        }

        if (!matched) {
            selection.groups = collect_default_groups(groups);
            return selection;
        }

        selection.active_profile = matched->name;
        for (const auto& group : groups) {
            if (!group.profile) {
                continue;
            }
            if (*group.profile == matched->name) {
                selection.groups.push_back(group);
            }
        }

        if (selection.groups.empty()) {
            selection.groups        = collect_default_groups(groups);
            selection.used_fallback = !selection.groups.empty();
        }
        return selection;
    }

} // namespace hyprspaces
