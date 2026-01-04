#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/protocols/WaylandProtocol.hpp>

#include "hyprspaces/layout_restore_tracker.hpp"
#include "hyprspaces/toplevel_state_tracker.hpp"
#include "hyprspaces/protocol_session.hpp"
#include "hyprspaces/layout_tree.hpp"
#include "hyprspaces/session_protocol_state.hpp"

struct wl_client;
struct wl_resource;
class CFunctionHook;

namespace hyprspaces {

    class SessionProtocolServer : public IWaylandProtocol {
      public:
        explicit SessionProtocolServer(std::filesystem::path store_path, SessionIdGenerator generator = {});
        ~SessionProtocolServer();

        void                       bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) override;

        std::optional<std::string> save(std::string_view id);
        std::optional<std::string> restore(std::string_view id);
        void                       install_layout_hooks(void* handle);
        void                       maybe_restore_layout(const PHLWINDOW& window);
        void                       maybe_restore_window_state(const PHLWINDOW& window);
        void                       record_window_state(const PHLWINDOW& window);

        struct SessionResource {
            SessionProtocolServer* server   = nullptr;
            wl_resource*           resource = nullptr;
            wl_client*             client   = nullptr;
            std::string            id;
            bool                   inert = false;
        };

        struct ToplevelResource {
            SessionProtocolServer* server   = nullptr;
            wl_resource*           resource = nullptr;
            wl_resource*           toplevel = nullptr;
            std::string            session_id;
            std::string            name;
            bool                   inert    = false;
            bool                   restored = false;
        };

        void                             handle_session_destroy(SessionResource* session, bool remove);
        void                             handle_toplevel_destroy(ToplevelResource* toplevel, bool remove);
        void                             mark_session_replaced(std::string_view id);
        void                             on_session_resource_destroy(SessionResource* session);
        void                             on_toplevel_resource_destroy(ToplevelResource* toplevel);
        void                             register_session(std::unique_ptr<SessionResource> session);
        void                             register_toplevel(std::unique_ptr<ToplevelResource> toplevel);
        bool                             add_toplevel(std::string_view session_id, std::string_view name, std::string* error);
        bool                             remove_toplevel(std::string_view session_id, std::string_view name);
        std::optional<SessionOpenResult> open_session(void* client, std::optional<std::string_view> requested_id, SessionReason reason, std::string* error);

      private:
        SessionProtocolState                           state_;
        std::filesystem::path                          store_path_;
        std::vector<std::unique_ptr<SessionResource>>  sessions_;
        std::vector<std::unique_ptr<ToplevelResource>> toplevels_;
        LayoutRestoreTracker<PHLWINDOW>                layout_tracker_;
        ToplevelStateTracker                           toplevel_state_tracker_;
        std::vector<CFunctionHook*>                    layout_hooks_;
        void*                                          hook_handle_ = nullptr;
    };

} // namespace hyprspaces
