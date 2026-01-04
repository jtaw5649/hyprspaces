#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hyprspaces/session_protocol_store.hpp"

namespace hyprspaces {

    class ToplevelStateTracker {
      public:
        void clear() {
            pending_.clear();
        }

        void clear_session(std::string_view session_id) {
            if (session_id.empty()) {
                return;
            }
            pending_.erase(std::string(session_id));
        }

        void set_states(std::string session_id, std::vector<SessionProtocolStore::ToplevelState> states) {
            if (session_id.empty()) {
                return;
            }
            std::unordered_map<std::string, SessionProtocolStore::ToplevelState> by_name;
            for (auto& state : states) {
                if (state.name.empty()) {
                    continue;
                }
                by_name[state.name] = std::move(state);
            }
            if (by_name.empty()) {
                pending_.erase(session_id);
                return;
            }
            pending_[session_id] = std::move(by_name);
        }

        bool has_pending(std::string_view session_id) const {
            if (session_id.empty()) {
                return false;
            }
            return pending_.contains(std::string(session_id));
        }

        std::optional<SessionProtocolStore::ToplevelState> take_state(std::string_view session_id, std::string_view name) {
            if (session_id.empty() || name.empty()) {
                return std::nullopt;
            }
            auto it = pending_.find(std::string(session_id));
            if (it == pending_.end()) {
                return std::nullopt;
            }
            auto& states   = it->second;
            auto  state_it = states.find(std::string(name));
            if (state_it == states.end()) {
                return std::nullopt;
            }
            auto state = std::move(state_it->second);
            states.erase(state_it);
            if (states.empty()) {
                pending_.erase(it);
            }
            return state;
        }

      private:
        std::unordered_map<std::string, std::unordered_map<std::string, SessionProtocolStore::ToplevelState>> pending_;
    };

} // namespace hyprspaces
