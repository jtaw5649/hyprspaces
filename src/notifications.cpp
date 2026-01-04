#include "hyprspaces/notifications.hpp"

namespace hyprspaces {

    namespace {

        std::string_view action_label(SessionAction action) {
            switch (action) {
                case SessionAction::kSave: return "save";
                case SessionAction::kRestore: return "restore";
            }
            return "session";
        }

        std::string_view action_verb(SessionAction action, bool success) {
            if (!success) {
                return "failed";
            }
            switch (action) {
                case SessionAction::kSave: return "saved";
                case SessionAction::kRestore: return "restored";
            }
            return "completed";
        }

        std::string_view action_prep(SessionAction action) {
            switch (action) {
                case SessionAction::kSave: return "to ";
                case SessionAction::kRestore: return "from ";
            }
            return "";
        }

    } // namespace

    std::string session_notification_text(SessionAction action, bool success, std::string_view path, std::string_view detail) {
        std::string message = "[hyprspaces] Session ";
        if (!success) {
            message += action_label(action);
            message += " failed";
            if (!detail.empty()) {
                message += ": ";
                message += detail;
            }
            return message;
        }

        message += action_verb(action, true);
        if (!path.empty()) {
            message += " ";
            message += action_prep(action);
            message += path;
        }
        return message;
    }

} // namespace hyprspaces
