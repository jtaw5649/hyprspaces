#include "hyprspaces/history.hpp"

#include "hyprspaces/pairing.hpp"

namespace hyprspaces {

    void WorkspaceHistory::record(std::string_view monitor, int workspace_id, int offset) {
        if (monitor.empty() || workspace_id <= 0 || offset <= 0) {
            return;
        }
        const int base  = normalize_workspace(workspace_id, offset);
        auto&     entry = entries_[std::string(monitor)];
        if (entry.current && *entry.current == base) {
            return;
        }
        entry.previous = entry.current;
        entry.current  = base;
    }

    std::optional<int> WorkspaceHistory::current(std::string_view monitor) const {
        if (monitor.empty()) {
            return std::nullopt;
        }
        const auto it = entries_.find(std::string(monitor));
        if (it == entries_.end()) {
            return std::nullopt;
        }
        return it->second.current;
    }

    std::optional<int> WorkspaceHistory::previous(std::string_view monitor) const {
        if (monitor.empty()) {
            return std::nullopt;
        }
        const auto it = entries_.find(std::string(monitor));
        if (it == entries_.end()) {
            return std::nullopt;
        }
        return it->second.previous;
    }

} // namespace hyprspaces
