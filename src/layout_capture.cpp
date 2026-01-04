#include "hyprspaces/layout_capture.hpp"

#include <any>
#include <sstream>

#define private public
#include <hyprland/src/managers/LayoutManager.hpp>
#undef private

namespace hyprspaces::layout_capture {
    namespace {

        MasterOrientation orientation_from_hyprland(eOrientation orientation) {
            switch (orientation) {
                case ORIENTATION_LEFT: return MasterOrientation::kLeft;
                case ORIENTATION_TOP: return MasterOrientation::kTop;
                case ORIENTATION_RIGHT: return MasterOrientation::kRight;
                case ORIENTATION_BOTTOM: return MasterOrientation::kBottom;
                case ORIENTATION_CENTER: return MasterOrientation::kCenter;
            }
            return MasterOrientation::kLeft;
        }

        LayoutKind kind_from_layout(IHyprLayout* layout) {
            if (!layout) {
                return LayoutKind::kUnknown;
            }
            const auto name = layout->getLayoutName();
            if (name == "dwindle") {
                return LayoutKind::kDwindle;
            }
            if (name == "master") {
                return LayoutKind::kMaster;
            }
            return LayoutKind::kUnknown;
        }

    } // namespace

    std::optional<LayoutSnapshot> capture_layout_snapshot(const std::unordered_map<const void*, std::string>& window_names, std::string* error) {
        if (error) {
            error->clear();
        }
        if (!g_pLayoutManager) {
            if (error) {
                *error = "layout manager unavailable";
            }
            return std::nullopt;
        }

        LayoutSnapshot snapshot;
        snapshot.kind = kind_from_layout(g_pLayoutManager->getCurrentLayout());

        auto resolve_window = [&](const auto& window) -> std::optional<std::string> {
            if (!window) {
                return std::nullopt;
            }
            const auto it = window_names.find(window.get());
            if (it == window_names.end()) {
                return std::nullopt;
            }
            return it->second;
        };

        snapshot.dwindle = capture_dwindle_workspaces(g_pLayoutManager->m_dwindleLayout.m_dwindleNodesData, resolve_window, error);
        snapshot.master  = capture_master_workspaces(g_pLayoutManager->m_masterLayout.m_masterNodesData, g_pLayoutManager->m_masterLayout.m_masterWorkspacesData, resolve_window,
                                                     [](const auto& workspace) { return orientation_from_hyprland(workspace.orientation); });

        return snapshot;
    }

} // namespace hyprspaces::layout_capture
