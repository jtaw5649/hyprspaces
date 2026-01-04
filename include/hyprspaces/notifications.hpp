#ifndef HYPRSPACES_NOTIFICATIONS_HPP
#define HYPRSPACES_NOTIFICATIONS_HPP

#include <string>
#include <string_view>

namespace hyprspaces {

    enum class SessionAction {
        kSave,
        kRestore,
    };

    std::string session_notification_text(SessionAction action, bool success, std::string_view path, std::string_view detail);

} // namespace hyprspaces

#endif // HYPRSPACES_NOTIFICATIONS_HPP
