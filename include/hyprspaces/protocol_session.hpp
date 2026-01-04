#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "hyprspaces/config.hpp"

namespace hyprspaces {

    enum class SessionOpenState {
        kCreated,
        kRestored,
        kReplaced,
    };

    struct SessionOpenResult {
        std::string      id;
        SessionOpenState state          = SessionOpenState::kCreated;
        void*            replaced_owner = nullptr;
    };

    using SessionIdGenerator = std::function<std::string()>;

    class SessionRegistry {
      public:
        explicit SessionRegistry(SessionIdGenerator generator = {});

        std::optional<SessionOpenResult> open_session(void* client, std::optional<std::string_view> requested_id, SessionReason reason, std::string* error);
        bool                             remember_session(std::string_view id);
        bool                             remove_session(std::string_view id);
        void                             close_session(void* client, std::string_view id);
        bool                             reserve_toplevel_name(std::string_view session_id, std::string_view name, std::string* error);
        void                             release_toplevel_name(std::string_view session_id, std::string_view name);

      private:
        struct SessionRecord {
            std::string                     id;
            SessionReason                   reason = SessionReason::kSessionRestore;
            void*                           owner  = nullptr;
            std::unordered_set<std::string> toplevel_names;
        };

        std::unordered_map<std::string, SessionRecord> sessions_;
        SessionIdGenerator                             id_generator_;
    };

} // namespace hyprspaces
