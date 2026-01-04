#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "hyprspaces/layout_tree.hpp"

namespace hyprspaces {

    template <typename WindowHandle>
    struct LayoutRestoreReady {
        LayoutSnapshot                                snapshot;
        std::unordered_map<std::string, WindowHandle> windows;
    };

    template <typename WindowHandle>
    class LayoutRestoreTracker {
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

        void set_snapshot(std::string session_id, LayoutSnapshot snapshot) {
            if (session_id.empty()) {
                return;
            }
            auto names = collect_snapshot_names(snapshot);
            if (names.empty()) {
                pending_.erase(session_id);
                return;
            }
            pending_[session_id] = Pending{
                .snapshot      = std::move(snapshot),
                .pending_names = std::move(names),
                .windows       = {},
            };
        }

        bool has_pending(std::string_view session_id) const {
            if (session_id.empty()) {
                return false;
            }
            return pending_.contains(std::string(session_id));
        }

        std::optional<LayoutRestoreReady<WindowHandle>> record_window(std::string_view session_id, std::string_view name, WindowHandle window) {
            if (session_id.empty() || name.empty()) {
                return std::nullopt;
            }
            auto it = pending_.find(std::string(session_id));
            if (it == pending_.end()) {
                return std::nullopt;
            }
            auto&             entry = it->second;
            const std::string key(name);
            if (!entry.pending_names.contains(key) && !entry.windows.contains(key)) {
                return std::nullopt;
            }
            entry.windows[key] = std::move(window);
            entry.pending_names.erase(key);
            if (!entry.pending_names.empty()) {
                return std::nullopt;
            }

            LayoutRestoreReady<WindowHandle> ready{
                .snapshot = entry.snapshot,
                .windows  = std::move(entry.windows),
            };
            pending_.erase(it);
            return ready;
        }

      private:
        struct Pending {
            LayoutSnapshot                                snapshot;
            std::unordered_set<std::string>               pending_names;
            std::unordered_map<std::string, WindowHandle> windows;
        };

        static void collect_dwindle_names(const LayoutNode& node, std::unordered_set<std::string>* names) {
            if (!names) {
                return;
            }
            if (node.toplevel.has_value() && !node.toplevel->empty()) {
                names->insert(*node.toplevel);
            }
            if (node.first) {
                collect_dwindle_names(*node.first, names);
            }
            if (node.second) {
                collect_dwindle_names(*node.second, names);
            }
        }

        static std::unordered_set<std::string> collect_snapshot_names(const LayoutSnapshot& snapshot) {
            std::unordered_set<std::string> names;
            if (snapshot.kind == LayoutKind::kDwindle) {
                for (const auto& workspace : snapshot.dwindle) {
                    if (!workspace.root) {
                        continue;
                    }
                    collect_dwindle_names(*workspace.root, &names);
                }
                return names;
            }
            if (snapshot.kind == LayoutKind::kMaster) {
                for (const auto& workspace : snapshot.master) {
                    for (const auto& node : workspace.nodes) {
                        if (!node.toplevel.empty()) {
                            names.insert(node.toplevel);
                        }
                    }
                }
            }
            return names;
        }

        std::unordered_map<std::string, Pending> pending_;
    };

} // namespace hyprspaces
