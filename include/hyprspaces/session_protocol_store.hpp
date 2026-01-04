#ifndef HYPRSPACES_SESSION_PROTOCOL_STORE_HPP
#define HYPRSPACES_SESSION_PROTOCOL_STORE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "hyprspaces/layout_tree.hpp"
#include "hyprspaces/types.hpp"

namespace hyprspaces {

    class SessionProtocolStore {
      public:
        struct ToplevelState {
            std::string                    name;
            std::optional<WindowGeometry>  geometry   = std::nullopt;
            std::optional<bool>            floating   = std::nullopt;
            std::optional<bool>            pinned     = std::nullopt;
            std::optional<FullscreenState> fullscreen = std::nullopt;
        };

        struct SessionRecord {
            std::string                   id;
            std::vector<ToplevelState>    toplevels;
            std::optional<LayoutSnapshot> layout = std::nullopt;
        };

        static std::optional<SessionProtocolStore> load(const std::filesystem::path& path, std::string* error);

        bool                                       save(const std::filesystem::path& path, std::string* error) const;

        bool                                       remember_session(std::string id);
        bool                                       remove_session(std::string_view id);
        bool                                       add_toplevel(std::string_view session_id, std::string_view name);
        bool                                       remove_toplevel(std::string_view session_id, std::string_view name);
        bool                                       update_toplevel_state(std::string_view session_id, std::string_view name, const ToplevelState& state);
        bool                                       update_layout_snapshot(std::string_view session_id, const LayoutSnapshot& snapshot);

        bool                                       has_session(std::string_view id) const;
        std::vector<std::string>                   session_ids() const;
        std::optional<std::vector<ToplevelState>>  toplevels_for(std::string_view id) const;
        std::optional<LayoutSnapshot>              layout_for(std::string_view id) const;

      private:
        std::unordered_map<std::string, SessionRecord> sessions_;
    };

} // namespace hyprspaces

#endif // HYPRSPACES_SESSION_PROTOCOL_STORE_HPP
