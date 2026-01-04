#include "hyprspaces/command.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hyprspaces {

    namespace {

        std::optional<std::string> split_tokens(std::string_view args, std::vector<std::string>* tokens) {
            std::string current;
            bool        in_quotes = false;
            bool        escaped   = false;
            char        quote     = '\0';
            for (const char ch : args) {
                if (escaped) {
                    current.push_back(ch);
                    escaped = false;
                    continue;
                }
                if (in_quotes && ch == '\\') {
                    escaped = true;
                    continue;
                }
                if (in_quotes) {
                    if (ch == quote) {
                        in_quotes = false;
                        continue;
                    }
                    current.push_back(ch);
                    continue;
                }
                if (ch == '"' || ch == '\'') {
                    in_quotes = true;
                    quote     = ch;
                    continue;
                }
                if (std::isspace(static_cast<unsigned char>(ch))) {
                    if (!current.empty()) {
                        tokens->push_back(current);
                        current.clear();
                    }
                    continue;
                }
                current.push_back(ch);
            }
            if (escaped || in_quotes) {
                return std::string("unterminated quote");
            }
            if (!current.empty()) {
                tokens->push_back(std::move(current));
            }
            return std::nullopt;
        }

    } // namespace

    std::variant<Command, ParseError> parse_command(std::string_view args) {
        std::vector<std::string> tokens;
        if (const auto error = split_tokens(args, &tokens)) {
            return ParseError{*error};
        }
        if (tokens.empty()) {
            return ParseError{"missing command"};
        }

        if (tokens[0] == "status") {
            return Command{.kind = CommandKind::kStatus};
        }

        if (tokens[0] == "paired") {
            if (tokens.size() < 2) {
                return ParseError{"missing paired subcommand"};
            }
            if (tokens[1] == "switch") {
                if (tokens.size() < 3) {
                    return ParseError{"missing workspace id"};
                }
                if (tokens.size() > 3) {
                    return ParseError{"unexpected extra arguments"};
                }
                return Command{.kind = CommandKind::kPairedSwitch, .workspace_spec = tokens[2]};
            }
            if (tokens[1] == "cycle") {
                if (tokens.size() < 3) {
                    return ParseError{"missing cycle direction"};
                }
                if (tokens.size() > 3) {
                    return ParseError{"unexpected extra arguments"};
                }
                if (tokens[2] == "next") {
                    return Command{.kind = CommandKind::kPairedCycle, .direction = CycleDirection::kNext};
                }
                if (tokens[2] == "prev") {
                    return Command{.kind = CommandKind::kPairedCycle, .direction = CycleDirection::kPrev};
                }
                return ParseError{"invalid cycle direction"};
            }
            if (tokens[1] == "move-window") {
                if (tokens.size() < 3) {
                    return ParseError{"missing workspace id"};
                }
                if (tokens.size() > 3) {
                    return ParseError{"unexpected extra arguments"};
                }
                return Command{.kind = CommandKind::kPairedMoveWindow, .workspace_spec = tokens[2]};
            }
            return ParseError{"unknown paired subcommand"};
        }

        if (tokens[0] == "setup") {
            if (tokens.size() < 2) {
                return ParseError{"missing setup subcommand"};
            }
            if (tokens[1] == "install") {
                Command command{.kind = CommandKind::kSetupInstall};
                for (size_t i = 2; i < tokens.size(); ++i) {
                    if (tokens[i] == "--waybar") {
                        command.setup_waybar = true;
                        continue;
                    }
                    return ParseError{"unknown setup install option"};
                }
                return command;
            }
            if (tokens[1] == "uninstall") {
                return Command{.kind = CommandKind::kSetupUninstall};
            }
            return ParseError{"unknown setup subcommand"};
        }

        if (tokens[0] == "session") {
            if (tokens.size() < 2) {
                return ParseError{"missing session subcommand"};
            }
            if (tokens[1] == "save") {
                if (tokens.size() > 2) {
                    return ParseError{"unexpected extra arguments"};
                }
                return Command{.kind = CommandKind::kSessionSave};
            }
            if (tokens[1] == "restore") {
                if (tokens.size() > 2) {
                    return ParseError{"unexpected extra arguments"};
                }
                return Command{.kind = CommandKind::kSessionRestore};
            }
            if (tokens[1] == "profile") {
                if (tokens.size() < 3) {
                    return ParseError{"missing profile subcommand"};
                }
                if (tokens[2] == "save") {
                    if (tokens.size() != 4) {
                        return ParseError{"missing profile name"};
                    }
                    return Command{.kind = CommandKind::kSessionProfileSave, .profile_name = tokens[3]};
                }
                if (tokens[2] == "load") {
                    if (tokens.size() != 4) {
                        return ParseError{"missing profile name"};
                    }
                    return Command{.kind = CommandKind::kSessionProfileLoad, .profile_name = tokens[3]};
                }
                if (tokens[2] == "list") {
                    if (tokens.size() != 3) {
                        return ParseError{"unexpected extra arguments"};
                    }
                    return Command{.kind = CommandKind::kSessionProfileList};
                }
                return ParseError{"unknown profile subcommand"};
            }
            return ParseError{"unknown session subcommand"};
        }

        if (tokens[0] == "mark") {
            if (tokens.size() < 2) {
                return ParseError{"missing mark subcommand"};
            }
            if (tokens[1] == "set") {
                if (tokens.size() != 3) {
                    return ParseError{"missing mark name"};
                }
                return Command{.kind = CommandKind::kMarkSet, .mark_name = tokens[2]};
            }
            if (tokens[1] == "jump") {
                if (tokens.size() != 3) {
                    return ParseError{"missing mark name"};
                }
                return Command{.kind = CommandKind::kMarkJump, .mark_name = tokens[2]};
            }
            return ParseError{"unknown mark subcommand"};
        }

        if (tokens[0] == "waybar") {
            Command command{.kind = CommandKind::kWaybar};
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "--enable-waybar") {
                    command.waybar_enabled = true;
                    continue;
                }
                if (tokens[i] == "--theme-css") {
                    if (i + 1 >= tokens.size()) {
                        return ParseError{"missing theme css path"};
                    }
                    command.waybar_theme_css = tokens[i + 1];
                    ++i;
                    continue;
                }
                return ParseError{"unknown waybar option"};
            }
            if (!command.waybar_enabled) {
                return ParseError{"waybar output requires --enable-waybar"};
            }
            return command;
        }

        return ParseError{"unknown command"};
    }

} // namespace hyprspaces
