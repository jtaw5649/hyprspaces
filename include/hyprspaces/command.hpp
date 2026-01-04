#ifndef HYPRSPACES_COMMAND_HPP
#define HYPRSPACES_COMMAND_HPP

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "hyprspaces/pairing.hpp"

namespace hyprspaces {

    enum class CommandKind {
        kStatus,
        kPairedSwitch,
        kPairedCycle,
        kPairedMoveWindow,
        kSetupInstall,
        kSetupUninstall,
        kSessionSave,
        kSessionRestore,
        kSessionProfileSave,
        kSessionProfileLoad,
        kSessionProfileList,
        kMarkSet,
        kMarkJump,
        kWaybar,
    };

    struct Command {
        CommandKind                   kind;
        std::optional<std::string>    workspace_spec   = std::nullopt;
        std::optional<CycleDirection> direction        = std::nullopt;
        std::optional<std::string>    profile_name     = std::nullopt;
        std::optional<std::string>    mark_name        = std::nullopt;
        bool                          setup_waybar     = false;
        bool                          waybar_enabled   = false;
        std::optional<std::string>    waybar_theme_css = std::nullopt;
    };

    struct ParseError {
        std::string message;
    };

    std::variant<Command, ParseError> parse_command(std::string_view args);

} // namespace hyprspaces

#endif // HYPRSPACES_COMMAND_HPP
