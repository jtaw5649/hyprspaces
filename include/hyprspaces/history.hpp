#ifndef HYPRSPACES_HISTORY_HPP
#define HYPRSPACES_HISTORY_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace hyprspaces {

    struct WorkspaceHistoryEntry {
        std::optional<int> current;
        std::optional<int> previous;
    };

    class WorkspaceHistory {
      public:
        void               record(std::string_view monitor, int workspace_id, int offset);
        std::optional<int> current(std::string_view monitor) const;
        std::optional<int> previous(std::string_view monitor) const;

      private:
        std::unordered_map<std::string, WorkspaceHistoryEntry> entries_;
    };

} // namespace hyprspaces

#endif // HYPRSPACES_HISTORY_HPP
