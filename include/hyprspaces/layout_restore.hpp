#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hyprspaces/layout_tree.hpp"

namespace hyprspaces::layout_restore {

    template <typename NodePtr, typename WindowPtr, typename LayoutPtr, typename WindowLookup, typename NodeFactory>
    NodePtr build_dwindle_tree(const LayoutNode& snapshot, int workspace_id, LayoutPtr layout, WindowLookup window_lookup, NodeFactory make_node, std::vector<NodePtr>* nodes,
                               std::string* error) {
        if (!nodes) {
            if (error) {
                *error = "missing node output";
            }
            return NodePtr{};
        }

        auto node = make_node();
        if (!node) {
            if (error) {
                *error = "failed to allocate layout node";
            }
            return NodePtr{};
        }

        if constexpr (requires { node->self = node; }) {
            node->self = node;
        }
        if constexpr (requires { node->layout = layout; }) {
            node->layout = layout;
        }
        if constexpr (requires { node->workspaceID = workspace_id; }) {
            node->workspaceID = workspace_id;
        }
        if constexpr (requires { node->valid = true; }) {
            node->valid = true;
        }
        if constexpr (requires { node->splitTop = snapshot.split_top; }) {
            node->splitTop = snapshot.split_top;
        }
        if constexpr (requires { node->splitRatio = snapshot.split_ratio; }) {
            node->splitRatio = snapshot.split_ratio;
        }

        const bool has_children = snapshot.first || snapshot.second;
        if constexpr (requires { node->isNode = has_children; }) {
            node->isNode = has_children;
        }

        if (!has_children) {
            if (snapshot.toplevel.has_value()) {
                const auto window = window_lookup(*snapshot.toplevel);
                if (!window) {
                    if (error) {
                        *error = "missing window for layout node";
                    }
                    return NodePtr{};
                }
                if constexpr (requires { node->pWindow = *window; }) {
                    node->pWindow = *window;
                }
            }
        } else {
            if (snapshot.first) {
                const auto child = build_dwindle_tree<NodePtr, WindowPtr>(*(snapshot.first), workspace_id, layout, window_lookup, make_node, nodes, error);
                if (!child) {
                    return NodePtr{};
                }
                if constexpr (requires { node->children[0] = child; }) {
                    node->children[0] = child;
                }
                if constexpr (requires { child->pParent = node; }) {
                    child->pParent = node;
                }
            }
            if (snapshot.second) {
                const auto child = build_dwindle_tree<NodePtr, WindowPtr>(*(snapshot.second), workspace_id, layout, window_lookup, make_node, nodes, error);
                if (!child) {
                    return NodePtr{};
                }
                if constexpr (requires { node->children[1] = child; }) {
                    node->children[1] = child;
                }
                if constexpr (requires { child->pParent = node; }) {
                    child->pParent = node;
                }
            }
        }

        nodes->push_back(node);
        return node;
    }

    template <typename NodeT, typename WindowPtr, typename WindowLookup>
    std::optional<std::vector<NodeT>> build_master_nodes(const MasterWorkspaceSnapshot& snapshot, WindowLookup window_lookup, std::string* error) {
        std::vector<NodeT> nodes;
        nodes.reserve(snapshot.nodes.size());
        for (const auto& entry : snapshot.nodes) {
            if (entry.toplevel.empty()) {
                continue;
            }
            const auto window = window_lookup(entry.toplevel);
            if (!window) {
                if (error) {
                    *error = "missing window for master node";
                }
                return std::nullopt;
            }

            NodeT node{};
            if constexpr (requires { node.isMaster = entry.is_master; }) {
                node.isMaster = entry.is_master;
            }
            if constexpr (requires { node.percMaster = entry.perc_master; }) {
                node.percMaster = entry.perc_master;
            }
            if constexpr (requires { node.percSize = entry.perc_size; }) {
                node.percSize = entry.perc_size;
            }
            if constexpr (requires { node.workspaceID = snapshot.workspace_id; }) {
                node.workspaceID = snapshot.workspace_id;
            }
            if constexpr (requires { node.pWindow = *window; }) {
                node.pWindow = *window;
            }
            nodes.push_back(std::move(node));
        }
        return nodes;
    }

} // namespace hyprspaces::layout_restore
