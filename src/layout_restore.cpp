#include "layout_restore.hpp"

#include <algorithm>
#include <any>
#include <sstream>

#include "hyprspaces/layout_restore.hpp"

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#undef private

namespace hyprspaces {
    namespace {

        eOrientation orientation_to_hyprland(MasterOrientation orientation) {
            switch (orientation) {
                case MasterOrientation::kLeft: return ORIENTATION_LEFT;
                case MasterOrientation::kTop: return ORIENTATION_TOP;
                case MasterOrientation::kRight: return ORIENTATION_RIGHT;
                case MasterOrientation::kBottom: return ORIENTATION_BOTTOM;
                case MasterOrientation::kCenter: return ORIENTATION_CENTER;
            }
            return ORIENTATION_LEFT;
        }

        std::optional<std::string> apply_dwindle_snapshot(const LayoutSnapshot& snapshot, const std::unordered_map<std::string, PHLWINDOW>& windows) {
            auto* layout = &g_pLayoutManager->m_dwindleLayout;
            auto& nodes  = layout->m_dwindleNodesData;

            for (const auto& workspace : snapshot.dwindle) {
                if (!workspace.root || workspace.workspace_id <= 0) {
                    continue;
                }
                std::erase_if(nodes, [&](const auto& node) { return node && node->workspaceID == workspace.workspace_id; });

                std::vector<SP<SDwindleNodeData>> created;
                std::string                       error;
                const auto                        root = layout_restore::build_dwindle_tree<SP<SDwindleNodeData>, PHLWINDOW>(
                    *workspace.root, workspace.workspace_id, layout,
                    [&](const std::string& name) -> std::optional<PHLWINDOW> {
                        const auto it = windows.find(name);
                        if (it == windows.end()) {
                            return std::nullopt;
                        }
                        return it->second;
                    },
                    []() { return makeShared<SDwindleNodeData>(); }, &created, &error);

                if (!root) {
                    return error.empty() ? std::optional<std::string>("failed to restore dwindle layout") : std::optional<std::string>(error);
                }

                std::move(created.begin(), created.end(), std::back_inserter(nodes));
                root->applyRootBox();
                root->recalcSizePosRecursive(true);
            }
            return std::nullopt;
        }

        std::optional<std::string> apply_master_snapshot(const LayoutSnapshot& snapshot, const std::unordered_map<std::string, PHLWINDOW>& windows) {
            auto* layout = &g_pLayoutManager->m_masterLayout;
            auto& nodes  = layout->m_masterNodesData;

            for (const auto& workspace : snapshot.master) {
                if (workspace.workspace_id <= 0) {
                    continue;
                }

                auto sorted = workspace;
                std::ranges::sort(sorted.nodes, [](const auto& a, const auto& b) { return a.index < b.index; });

                std::string error;
                const auto  rebuilt = layout_restore::build_master_nodes<SMasterNodeData, PHLWINDOW>(
                    sorted,
                    [&](const std::string& name) -> std::optional<PHLWINDOW> {
                        const auto it = windows.find(name);
                        if (it == windows.end()) {
                            return std::nullopt;
                        }
                        return it->second;
                    },
                    &error);

                if (!rebuilt) {
                    return error.empty() ? std::optional<std::string>("failed to restore master layout") : std::optional<std::string>(error);
                }

                std::erase_if(nodes, [&](const auto& node) { return node.workspaceID == workspace.workspace_id; });
                for (auto& node : *rebuilt) {
                    nodes.push_back(std::move(node));
                }

                auto it = std::ranges::find_if(layout->m_masterWorkspacesData, [&](const auto& data) { return data.workspaceID == workspace.workspace_id; });
                if (it != layout->m_masterWorkspacesData.end()) {
                    it->orientation = orientation_to_hyprland(workspace.orientation);
                } else {
                    layout->m_masterWorkspacesData.push_back(SMasterWorkspaceData{
                        .workspaceID     = workspace.workspace_id,
                        .orientation     = orientation_to_hyprland(workspace.orientation),
                        .focusMasterPrev = {},
                    });
                }

                const auto ws = g_pCompositor->getWorkspaceByID(workspace.workspace_id);
                if (ws) {
                    layout->calculateWorkspace(ws);
                    layout->recalculateMonitor(ws->monitorID());
                }
            }

            return std::nullopt;
        }

    } // namespace

    std::optional<std::string> apply_layout_snapshot(const LayoutSnapshot& snapshot, const std::unordered_map<std::string, PHLWINDOW>& windows) {
        if (!g_pLayoutManager || !g_pCompositor) {
            return std::string("layout manager unavailable");
        }
        switch (snapshot.kind) {
            case LayoutKind::kDwindle: return apply_dwindle_snapshot(snapshot, windows);
            case LayoutKind::kMaster: return apply_master_snapshot(snapshot, windows);
            case LayoutKind::kUnknown: break;
        }
        return std::string("unsupported layout snapshot");
    }

} // namespace hyprspaces
