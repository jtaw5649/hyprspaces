#ifndef HYPRSPACES_SESSION_PROTOCOL_STATE_HPP
#define HYPRSPACES_SESSION_PROTOCOL_STATE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "hyprspaces/protocol_session.hpp"
#include "hyprspaces/session_protocol_store.hpp"

namespace hyprspaces {

    class SessionProtocolState {
      public:
        explicit SessionProtocolState(SessionIdGenerator generator = {});

        bool                             load_store(const std::filesystem::path& path, std::string* error);
        bool                             save_store(const std::filesystem::path& path, std::string* error) const;

        std::optional<SessionOpenResult> open_session(void* client, std::optional<std::string_view> requested_id, SessionReason reason, std::string* error);
        void                             close_session(void* client, std::string_view id);
        bool                             remove_session(std::string_view id);

        bool                             add_toplevel(std::string_view session_id, std::string_view name, std::string* error);
        bool                             remove_toplevel(std::string_view session_id, std::string_view name);
        void                             release_toplevel(std::string_view session_id, std::string_view name);
        bool                             update_toplevel_state(std::string_view session_id, std::string_view name, const SessionProtocolStore::ToplevelState& state);
        bool                             update_layout_snapshot(std::string_view session_id, const LayoutSnapshot& snapshot);

        const SessionProtocolStore&      store() const;

      private:
        SessionRegistry      registry_;
        SessionProtocolStore store_;
    };

} // namespace hyprspaces

#endif // HYPRSPACES_SESSION_PROTOCOL_STATE_HPP
