#pragma once

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "hyprspaces/layout_tree.hpp"

namespace hyprspaces::layout_capture {

    namespace detail {

        template <typename NodeT>
        bool node_is_valid(const NodeT& node) {
            if constexpr (requires { node.valid; }) {
                return node.valid;
            }
            return true;
        }

    } // namespace detail

    template <typename NodePtr, typename WindowNameResolver>
    std::unique_ptr<LayoutNode> build_dwindle_node(const NodePtr& node, WindowNameResolver resolve_window_name) {
        if (!node) {
            return nullptr;
        }
        auto out         = std::make_unique<LayoutNode>();
        out->split_top   = node->splitTop;
        out->split_ratio = node->splitRatio;

        if (!node->isNode) {
            const auto window = node->pWindow.lock();
            if (window) {
                const auto name = resolve_window_name(window);
                if (name.has_value() && !name->empty()) {
                    out->toplevel = *name;
                }
            }
            return out;
        }

        const auto first  = node->children[0].lock();
        const auto second = node->children[1].lock();
        out->first        = build_dwindle_node(first, resolve_window_name);
        out->second       = build_dwindle_node(second, resolve_window_name);
        return out;
    }

    template <typename NodePtr, typename WindowNameResolver>
    std::vector<DwindleWorkspaceSnapshot> capture_dwindle_workspaces(const std::vector<NodePtr>& nodes, WindowNameResolver resolve_window_name, std::string* error) {
        if (error) {
            error->clear();
        }
        std::vector<int> workspace_ids;
        workspace_ids.reserve(nodes.size());
        for (const auto& node : nodes) {
            if (!node || !detail::node_is_valid(*node)) {
                continue;
            }
            const auto id = static_cast<int>(node->workspaceID);
            if (id <= 0) {
                continue;
            }
            workspace_ids.push_back(id);
        }
        std::ranges::sort(workspace_ids);
        workspace_ids.erase(std::unique(workspace_ids.begin(), workspace_ids.end()), workspace_ids.end());

        std::vector<DwindleWorkspaceSnapshot> snapshots;
        snapshots.reserve(workspace_ids.size());
        for (const auto workspace_id : workspace_ids) {
            NodePtr root;
            for (const auto& node : nodes) {
                if (!node || !detail::node_is_valid(*node)) {
                    continue;
                }
                if (static_cast<int>(node->workspaceID) != workspace_id) {
                    continue;
                }
                if (node->pParent.lock()) {
                    continue;
                }
                root = node;
                break;
            }
            if (!root) {
                continue;
            }
            DwindleWorkspaceSnapshot workspace;
            workspace.workspace_id = workspace_id;
            workspace.root         = build_dwindle_node(root, resolve_window_name);
            snapshots.push_back(std::move(workspace));
        }
        return snapshots;
    }

    template <typename NodeT, typename WorkspaceT, typename WindowNameResolver, typename OrientationGetter>
    std::vector<MasterWorkspaceSnapshot> capture_master_workspaces(const std::list<NodeT>& nodes, const std::vector<WorkspaceT>& workspaces, WindowNameResolver resolve_window_name,
                                                                   OrientationGetter orientation_getter) {
        std::unordered_map<int, MasterOrientation> orientation_by_workspace;
        orientation_by_workspace.reserve(workspaces.size());
        for (const auto& workspace : workspaces) {
            const auto id                = static_cast<int>(workspace.workspaceID);
            orientation_by_workspace[id] = orientation_getter(workspace);
        }

        std::unordered_map<int, int>           index_by_workspace;
        std::map<int, MasterWorkspaceSnapshot> snapshot_by_workspace;

        for (const auto& node : nodes) {
            const auto id = static_cast<int>(node.workspaceID);
            if (id <= 0) {
                continue;
            }
            const auto window = node.pWindow.lock();
            if (!window) {
                continue;
            }
            const auto name = resolve_window_name(window);
            if (!name.has_value() || name->empty()) {
                continue;
            }

            auto& snapshot = snapshot_by_workspace[id];
            if (snapshot.workspace_id == 0) {
                snapshot.workspace_id = id;
                const auto found      = orientation_by_workspace.find(id);
                snapshot.orientation  = found != orientation_by_workspace.end() ? found->second : MasterOrientation::kLeft;
            }
            const auto index = index_by_workspace[id]++;
            snapshot.nodes.push_back(MasterNodeSnapshot{
                .toplevel    = *name,
                .is_master   = node.isMaster,
                .perc_master = node.percMaster,
                .perc_size   = node.percSize,
                .index       = index,
            });
        }

        std::vector<MasterWorkspaceSnapshot> snapshots;
        snapshots.reserve(snapshot_by_workspace.size());
        for (auto& [id, snapshot] : snapshot_by_workspace) {
            snapshots.push_back(std::move(snapshot));
        }
        return snapshots;
    }

    std::optional<LayoutSnapshot> capture_layout_snapshot(const std::unordered_map<const void*, std::string>& window_names, std::string* error);

} // namespace hyprspaces::layout_capture
