#include "hyprspaces/monitor_groups.hpp"

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

        int parse_int(std::string_view value, std::string_view label) {
            const auto trimmed = trim_copy(value);
            if (trimmed.empty()) {
                throw std::runtime_error(std::string(label) + " value is empty");
            }
            try {
                return std::stoi(trimmed);
            } catch (const std::exception&) { throw std::runtime_error(std::string("invalid ") + std::string(label)); }
        }

        std::pair<std::string, std::string> split_key_value(std::string_view token) {
            const auto colon = token.find(':');
            if (colon == std::string_view::npos) {
                throw std::runtime_error("monitor group entry missing ':'");
            }
            const auto key   = trim_copy(token.substr(0, colon));
            const auto value = trim_copy(token.substr(colon + 1));
            if (key.empty() || value.empty()) {
                throw std::runtime_error("monitor group entry missing key/value");
            }
            return {key, value};
        }

    } // namespace

    MonitorGroup parse_monitor_group(std::string_view value) {
        MonitorGroup group;
        const auto   tokens = split_tokens(value, ',');

        for (const auto& raw : tokens) {
            if (raw.empty()) {
                throw std::runtime_error("monitor group entry is empty");
            }
            const auto [key, val] = split_key_value(raw);
            if (key == "profile") {
                group.profile = val;
                continue;
            }
            if (key == "name") {
                group.name = val;
                continue;
            }
            if (key == "monitors") {
                const auto monitors = split_tokens(val, ';');
                for (const auto& monitor : monitors) {
                    if (monitor.empty()) {
                        throw std::runtime_error("monitor name is empty");
                    }
                    group.monitors.push_back(monitor);
                }
                continue;
            }
            if (key == "workspace_offset") {
                group.workspace_offset = parse_int(val, "workspace_offset");
                if (group.workspace_offset < 0) {
                    throw std::runtime_error("workspace_offset must be non-negative");
                }
                continue;
            }
            throw std::runtime_error("unknown monitor group field");
        }

        if (group.name.empty()) {
            throw std::runtime_error("monitor group name is required");
        }
        if (group.monitors.empty()) {
            throw std::runtime_error("monitor group monitors are required");
        }
        return group;
    }

    std::optional<MonitorGroupMatch> find_group_for_monitor(std::string_view monitor, const std::vector<MonitorGroup>& groups) {
        if (monitor.empty()) {
            return std::nullopt;
        }
        for (const auto& group : groups) {
            for (size_t i = 0; i < group.monitors.size(); ++i) {
                if (group.monitors[i] == monitor) {
                    return MonitorGroupMatch{.group = &group, .monitor_index = static_cast<int>(i), .normalized_workspace = 0};
                }
            }
        }
        return std::nullopt;
    }

    std::optional<MonitorGroupMatch> find_group_for_workspace(int workspace_id, int workspace_count, const std::vector<MonitorGroup>& groups) {
        if (workspace_id <= 0 || workspace_count <= 0) {
            return std::nullopt;
        }
        for (const auto& group : groups) {
            if (group.monitors.empty()) {
                continue;
            }
            const int relative = workspace_id - group.workspace_offset;
            if (relative <= 0) {
                continue;
            }
            const int capacity = workspace_count * static_cast<int>(group.monitors.size());
            if (relative > capacity) {
                continue;
            }
            const int index      = (relative - 1) / workspace_count;
            const int normalized = ((relative - 1) % workspace_count) + 1;
            if (index < 0 || index >= static_cast<int>(group.monitors.size())) {
                continue;
            }
            return MonitorGroupMatch{.group = &group, .monitor_index = index, .normalized_workspace = normalized};
        }
        return std::nullopt;
    }

    std::optional<int> workspace_id_for_group(const MonitorGroup& group, int workspace_count, int monitor_index, int normalized_workspace) {
        if (workspace_count <= 0 || normalized_workspace <= 0) {
            return std::nullopt;
        }
        if (monitor_index < 0 || monitor_index >= static_cast<int>(group.monitors.size())) {
            return std::nullopt;
        }
        if (normalized_workspace > workspace_count) {
            return std::nullopt;
        }
        return group.workspace_offset + (monitor_index * workspace_count) + normalized_workspace;
    }

} // namespace hyprspaces
