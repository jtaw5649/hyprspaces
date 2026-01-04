#ifndef HYPRSPACES_LAYOUT_TREE_HPP
#define HYPRSPACES_LAYOUT_TREE_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hyprspaces {

    enum class LayoutKind {
        kUnknown,
        kDwindle,
        kMaster,
    };

    struct LayoutNode {
        LayoutNode() = default;
        LayoutNode(const LayoutNode& other);
        LayoutNode& operator=(const LayoutNode& other);
        LayoutNode(LayoutNode&&) noexcept            = default;
        LayoutNode& operator=(LayoutNode&&) noexcept = default;
        ~LayoutNode()                                = default;

        std::optional<std::string>  toplevel    = std::nullopt;
        bool                        split_top   = false;
        float                       split_ratio = 1.f;
        std::unique_ptr<LayoutNode> first;
        std::unique_ptr<LayoutNode> second;
    };

    struct DwindleWorkspaceSnapshot {
        DwindleWorkspaceSnapshot() = default;
        DwindleWorkspaceSnapshot(const DwindleWorkspaceSnapshot& other);
        DwindleWorkspaceSnapshot& operator=(const DwindleWorkspaceSnapshot& other);
        DwindleWorkspaceSnapshot(DwindleWorkspaceSnapshot&&) noexcept            = default;
        DwindleWorkspaceSnapshot& operator=(DwindleWorkspaceSnapshot&&) noexcept = default;
        ~DwindleWorkspaceSnapshot()                                              = default;

        int                         workspace_id = 0;
        std::unique_ptr<LayoutNode> root;
    };

    enum class MasterOrientation {
        kLeft,
        kTop,
        kRight,
        kBottom,
        kCenter,
    };

    struct MasterNodeSnapshot {
        std::string toplevel;
        bool        is_master   = false;
        float       perc_master = 0.5f;
        float       perc_size   = 1.f;
        int         index       = 0;
    };

    struct MasterWorkspaceSnapshot {
        int                             workspace_id = 0;
        MasterOrientation               orientation  = MasterOrientation::kLeft;
        std::vector<MasterNodeSnapshot> nodes;
    };

    struct LayoutSnapshot {
        LayoutKind                            kind = LayoutKind::kUnknown;
        std::vector<DwindleWorkspaceSnapshot> dwindle;
        std::vector<MasterWorkspaceSnapshot>  master;
    };

    struct LayoutSnapshotJson {
        std::string json;
    };

    std::optional<LayoutSnapshotJson> layout_snapshot_to_json(const LayoutSnapshot& snapshot, std::string* error);
    std::optional<LayoutSnapshot>     layout_snapshot_from_json(std::string_view json_text, std::string* error);

} // namespace hyprspaces

#endif // HYPRSPACES_LAYOUT_TREE_HPP
