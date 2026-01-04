#include "hyprspaces/protocol_session.hpp"

#include <array>
#include <random>
#include <sstream>

namespace hyprspaces {

    namespace {

        std::string default_session_id() {
            std::array<unsigned char, 16> bytes{};
            std::random_device            rng;
            for (auto& byte : bytes) {
                byte = static_cast<unsigned char>(rng());
            }

            std::ostringstream out;
            out << std::hex;
            for (const auto byte : bytes) {
                out.width(2);
                out.fill('0');
                out << static_cast<int>(byte);
            }
            return out.str();
        }

    } // namespace

    SessionRegistry::SessionRegistry(SessionIdGenerator generator) : id_generator_(std::move(generator)) {
        if (!id_generator_) {
            id_generator_ = default_session_id;
        }
    }

    std::optional<SessionOpenResult> SessionRegistry::open_session(void* client, std::optional<std::string_view> requested_id, SessionReason reason, std::string* error) {
        if (error) {
            error->clear();
        }
        if (!client) {
            if (error) {
                *error = "missing client";
            }
            return std::nullopt;
        }

        if (requested_id && !requested_id->empty()) {
            const auto it = sessions_.find(std::string(*requested_id));
            if (it != sessions_.end()) {
                if (it->second.owner == client) {
                    if (error) {
                        *error = "session in use";
                    }
                    return std::nullopt;
                }
                if (!it->second.owner) {
                    it->second.owner  = client;
                    it->second.reason = reason;
                    return SessionOpenResult{.id = it->second.id, .state = SessionOpenState::kRestored, .replaced_owner = nullptr};
                }
                SessionOpenResult result{
                    .id             = it->second.id,
                    .state          = SessionOpenState::kReplaced,
                    .replaced_owner = it->second.owner,
                };
                it->second.owner  = client;
                it->second.reason = reason;
                return result;
            }
        }

        const auto id = id_generator_ ? id_generator_() : std::string{};
        if (id.empty()) {
            if (error) {
                *error = "session id unavailable";
            }
            return std::nullopt;
        }

        sessions_[id] = SessionRecord{
            .id             = id,
            .reason         = reason,
            .owner          = client,
            .toplevel_names = {},
        };
        return SessionOpenResult{.id = id, .state = SessionOpenState::kCreated, .replaced_owner = nullptr};
    }

    bool SessionRegistry::remember_session(std::string_view id) {
        if (id.empty()) {
            return false;
        }
        const auto key = std::string(id);
        if (sessions_.contains(key)) {
            return false;
        }
        sessions_.emplace(key, SessionRecord{.id = key, .toplevel_names = {}});
        return true;
    }

    bool SessionRegistry::remove_session(std::string_view id) {
        if (id.empty()) {
            return false;
        }
        return sessions_.erase(std::string(id)) > 0;
    }

    void SessionRegistry::close_session(void* client, std::string_view id) {
        if (!client || id.empty()) {
            return;
        }
        const auto it = sessions_.find(std::string(id));
        if (it == sessions_.end()) {
            return;
        }
        if (it->second.owner == client) {
            it->second.owner = nullptr;
            it->second.toplevel_names.clear();
        }
    }

    bool SessionRegistry::reserve_toplevel_name(std::string_view session_id, std::string_view name, std::string* error) {
        if (error) {
            error->clear();
        }
        if (session_id.empty() || name.empty()) {
            if (error) {
                *error = "invalid name";
            }
            return false;
        }
        const auto it = sessions_.find(std::string(session_id));
        if (it == sessions_.end()) {
            if (error) {
                *error = "unknown session";
            }
            return false;
        }
        const auto [_, inserted] = it->second.toplevel_names.emplace(std::string(name));
        if (!inserted && error) {
            *error = "name in use";
        }
        return inserted;
    }

    void SessionRegistry::release_toplevel_name(std::string_view session_id, std::string_view name) {
        if (session_id.empty() || name.empty()) {
            return;
        }
        const auto it = sessions_.find(std::string(session_id));
        if (it == sessions_.end()) {
            return;
        }
        it->second.toplevel_names.erase(std::string(name));
    }

} // namespace hyprspaces
