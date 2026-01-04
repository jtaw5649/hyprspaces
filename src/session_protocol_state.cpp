#include "hyprspaces/session_protocol_state.hpp"

namespace hyprspaces {

    SessionProtocolState::SessionProtocolState(SessionIdGenerator generator) : registry_(std::move(generator)) {}

    bool SessionProtocolState::load_store(const std::filesystem::path& path, std::string* error) {
        const auto loaded = SessionProtocolStore::load(path, error);
        if (!loaded.has_value()) {
            return false;
        }
        store_ = *loaded;
        for (const auto& id : store_.session_ids()) {
            registry_.remember_session(id);
        }
        return true;
    }

    bool SessionProtocolState::save_store(const std::filesystem::path& path, std::string* error) const {
        return store_.save(path, error);
    }

    std::optional<SessionOpenResult> SessionProtocolState::open_session(void* client, std::optional<std::string_view> requested_id, SessionReason reason, std::string* error) {
        auto result = registry_.open_session(client, requested_id, reason, error);
        if (!result.has_value()) {
            return std::nullopt;
        }
        if (!store_.has_session(result->id)) {
            store_.remember_session(result->id);
        }
        return result;
    }

    void SessionProtocolState::close_session(void* client, std::string_view id) {
        registry_.close_session(client, id);
    }

    bool SessionProtocolState::remove_session(std::string_view id) {
        const auto removed = store_.remove_session(id);
        registry_.remove_session(id);
        return removed;
    }

    bool SessionProtocolState::add_toplevel(std::string_view session_id, std::string_view name, std::string* error) {
        if (!registry_.reserve_toplevel_name(session_id, name, error)) {
            return false;
        }
        store_.add_toplevel(session_id, name);
        return true;
    }

    bool SessionProtocolState::remove_toplevel(std::string_view session_id, std::string_view name) {
        const auto removed = store_.remove_toplevel(session_id, name);
        registry_.release_toplevel_name(session_id, name);
        return removed;
    }

    void SessionProtocolState::release_toplevel(std::string_view session_id, std::string_view name) {
        registry_.release_toplevel_name(session_id, name);
    }

    bool SessionProtocolState::update_toplevel_state(std::string_view session_id, std::string_view name, const SessionProtocolStore::ToplevelState& state) {
        return store_.update_toplevel_state(session_id, name, state);
    }

    bool SessionProtocolState::update_layout_snapshot(std::string_view session_id, const LayoutSnapshot& snapshot) {
        return store_.update_layout_snapshot(session_id, snapshot);
    }

    const SessionProtocolStore& SessionProtocolState::store() const {
        return store_;
    }

} // namespace hyprspaces
