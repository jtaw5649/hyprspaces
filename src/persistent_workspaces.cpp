#include "hyprspaces/persistent_workspaces.hpp"

#include <string>

#include "hyprspaces/strings.hpp"

namespace hyprspaces {

    std::optional<int> parse_persistent_workspace(std::string_view value) {
        const auto trimmed = trim_view(value);
        if (trimmed.empty()) {
            return std::nullopt;
        }
        try {
            size_t    index  = 0;
            const int parsed = std::stoi(std::string(trimmed), &index, 10);
            if (index != trimmed.size()) {
                return std::nullopt;
            }
            if (parsed <= 0) {
                return std::nullopt;
            }
            return parsed;
        } catch (const std::exception&) { return std::nullopt; }
    }

} // namespace hyprspaces
