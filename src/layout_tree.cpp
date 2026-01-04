#include "hyprspaces/layout_tree.hpp"

#include <limits>

#include <nlohmann/json.hpp>

namespace hyprspaces {
    namespace {

        constexpr int               kLayoutSnapshotVersion = 1;

        std::unique_ptr<LayoutNode> clone_node(const LayoutNode* node) {
            if (!node) {
                return nullptr;
            }
            auto clone         = std::make_unique<LayoutNode>();
            clone->toplevel    = node->toplevel;
            clone->split_top   = node->split_top;
            clone->split_ratio = node->split_ratio;
            clone->first       = clone_node(node->first.get());
            clone->second      = clone_node(node->second.get());
            return clone;
        }

        std::string kind_to_string(LayoutKind kind) {
            switch (kind) {
                case LayoutKind::kDwindle: return "dwindle";
                case LayoutKind::kMaster: return "master";
                case LayoutKind::kUnknown: return "unknown";
            }
            return "unknown";
        }

        std::optional<LayoutKind> kind_from_string(const nlohmann::json& value) {
            if (!value.is_string()) {
                return std::nullopt;
            }
            const auto str = value.get<std::string>();
            if (str == "dwindle") {
                return LayoutKind::kDwindle;
            }
            if (str == "master") {
                return LayoutKind::kMaster;
            }
            if (str == "unknown") {
                return LayoutKind::kUnknown;
            }
            return std::nullopt;
        }

        std::string orientation_to_string(MasterOrientation orientation) {
            switch (orientation) {
                case MasterOrientation::kLeft: return "left";
                case MasterOrientation::kTop: return "top";
                case MasterOrientation::kRight: return "right";
                case MasterOrientation::kBottom: return "bottom";
                case MasterOrientation::kCenter: return "center";
            }
            return "left";
        }

        std::optional<MasterOrientation> orientation_from_string(const nlohmann::json& value) {
            if (!value.is_string()) {
                return std::nullopt;
            }
            const auto str = value.get<std::string>();
            if (str == "left") {
                return MasterOrientation::kLeft;
            }
            if (str == "top") {
                return MasterOrientation::kTop;
            }
            if (str == "right") {
                return MasterOrientation::kRight;
            }
            if (str == "bottom") {
                return MasterOrientation::kBottom;
            }
            if (str == "center") {
                return MasterOrientation::kCenter;
            }
            return std::nullopt;
        }

        void set_error(std::string* error, std::string message) {
            if (!error) {
                return;
            }
            *error = std::move(message);
        }

        nlohmann::json node_to_json(const LayoutNode& node) {
            nlohmann::json entry;
            if (node.toplevel.has_value()) {
                entry["toplevel"] = *node.toplevel;
            }
            entry["split_top"]   = node.split_top;
            entry["split_ratio"] = node.split_ratio;
            if (node.first) {
                entry["first"] = node_to_json(*node.first);
            }
            if (node.second) {
                entry["second"] = node_to_json(*node.second);
            }
            return entry;
        }

        std::unique_ptr<LayoutNode> node_from_json(const nlohmann::json& entry, std::string* error) {
            if (!entry.is_object()) {
                set_error(error, "layout node is not an object");
                return nullptr;
            }
            auto node = std::make_unique<LayoutNode>();
            if (entry.contains("toplevel")) {
                if (entry.at("toplevel").is_null()) {
                    node->toplevel = std::nullopt;
                } else if (entry.at("toplevel").is_string()) {
                    node->toplevel = entry.at("toplevel").get<std::string>();
                } else {
                    set_error(error, "layout node toplevel must be string");
                    return nullptr;
                }
            }
            if (entry.contains("split_top")) {
                if (!entry.at("split_top").is_boolean()) {
                    set_error(error, "layout node split_top must be boolean");
                    return nullptr;
                }
                node->split_top = entry.at("split_top").get<bool>();
            }
            if (entry.contains("split_ratio")) {
                if (!entry.at("split_ratio").is_number()) {
                    set_error(error, "layout node split_ratio must be number");
                    return nullptr;
                }
                node->split_ratio = entry.at("split_ratio").get<float>();
            }
            if (entry.contains("first")) {
                if (entry.at("first").is_null()) {
                    node->first.reset();
                } else {
                    node->first = node_from_json(entry.at("first"), error);
                    if (!node->first && error && !error->empty()) {
                        return nullptr;
                    }
                }
            }
            if (entry.contains("second")) {
                if (entry.at("second").is_null()) {
                    node->second.reset();
                } else {
                    node->second = node_from_json(entry.at("second"), error);
                    if (!node->second && error && !error->empty()) {
                        return nullptr;
                    }
                }
            }
            return node;
        }

    } // namespace

    LayoutNode::LayoutNode(const LayoutNode& other) :
        toplevel(other.toplevel), split_top(other.split_top), split_ratio(other.split_ratio), first(clone_node(other.first.get())), second(clone_node(other.second.get())) {}

    LayoutNode& LayoutNode::operator=(const LayoutNode& other) {
        if (this == &other) {
            return *this;
        }
        toplevel    = other.toplevel;
        split_top   = other.split_top;
        split_ratio = other.split_ratio;
        first       = clone_node(other.first.get());
        second      = clone_node(other.second.get());
        return *this;
    }

    DwindleWorkspaceSnapshot::DwindleWorkspaceSnapshot(const DwindleWorkspaceSnapshot& other) : workspace_id(other.workspace_id), root(clone_node(other.root.get())) {}

    DwindleWorkspaceSnapshot& DwindleWorkspaceSnapshot::operator=(const DwindleWorkspaceSnapshot& other) {
        if (this == &other) {
            return *this;
        }
        workspace_id = other.workspace_id;
        root         = clone_node(other.root.get());
        return *this;
    }

    std::optional<LayoutSnapshotJson> layout_snapshot_to_json(const LayoutSnapshot& snapshot, std::string* error) {
        if (error) {
            error->clear();
        }
        try {
            nlohmann::json root;
            root["version"] = kLayoutSnapshotVersion;
            root["kind"]    = kind_to_string(snapshot.kind);
            root["dwindle"] = nlohmann::json::array();
            for (const auto& workspace : snapshot.dwindle) {
                nlohmann::json entry;
                entry["workspace_id"] = workspace.workspace_id;
                if (workspace.root) {
                    entry["root"] = node_to_json(*workspace.root);
                } else {
                    entry["root"] = nullptr;
                }
                root["dwindle"].push_back(std::move(entry));
            }
            root["master"] = nlohmann::json::array();
            for (const auto& workspace : snapshot.master) {
                nlohmann::json entry;
                entry["workspace_id"] = workspace.workspace_id;
                entry["orientation"]  = orientation_to_string(workspace.orientation);
                entry["nodes"]        = nlohmann::json::array();
                for (const auto& node : workspace.nodes) {
                    nlohmann::json node_entry;
                    node_entry["toplevel"]    = node.toplevel;
                    node_entry["is_master"]   = node.is_master;
                    node_entry["perc_master"] = node.perc_master;
                    node_entry["perc_size"]   = node.perc_size;
                    node_entry["index"]       = node.index;
                    entry["nodes"].push_back(std::move(node_entry));
                }
                root["master"].push_back(std::move(entry));
            }
            LayoutSnapshotJson output{root.dump()};
            return output;
        } catch (const std::exception& exc) {
            set_error(error, std::string("layout snapshot encode failed: ") + exc.what());
            return std::nullopt;
        }
    }

    std::optional<LayoutSnapshot> layout_snapshot_from_json(std::string_view json_text, std::string* error) {
        if (error) {
            error->clear();
        }
        try {
            const auto root = nlohmann::json::parse(json_text.begin(), json_text.end(), nullptr, false);
            if (root.is_discarded()) {
                set_error(error, "layout snapshot invalid json");
                return std::nullopt;
            }
            if (!root.is_object()) {
                set_error(error, "layout snapshot root must be object");
                return std::nullopt;
            }
            const auto version = root.value("version", 0);
            if (version != kLayoutSnapshotVersion) {
                set_error(error, "layout snapshot version mismatch");
                return std::nullopt;
            }
            LayoutSnapshot snapshot;
            if (root.contains("kind")) {
                const auto parsed_kind = kind_from_string(root.at("kind"));
                if (!parsed_kind.has_value()) {
                    set_error(error, "layout snapshot kind is invalid");
                    return std::nullopt;
                }
                snapshot.kind = *parsed_kind;
            } else {
                snapshot.kind = LayoutKind::kUnknown;
            }
            const auto dwindle = root.value("dwindle", nlohmann::json::array());
            if (!dwindle.is_array()) {
                set_error(error, "layout snapshot dwindle must be array");
                return std::nullopt;
            }
            for (const auto& entry : dwindle) {
                if (!entry.is_object()) {
                    continue;
                }
                DwindleWorkspaceSnapshot workspace;
                workspace.workspace_id = entry.value("workspace_id", 0);
                if (entry.contains("root")) {
                    if (entry.at("root").is_null()) {
                        workspace.root.reset();
                    } else {
                        workspace.root = node_from_json(entry.at("root"), error);
                        if (!workspace.root && error && !error->empty()) {
                            return std::nullopt;
                        }
                    }
                }
                snapshot.dwindle.push_back(std::move(workspace));
            }
            const auto master = root.value("master", nlohmann::json::array());
            if (!master.is_array()) {
                set_error(error, "layout snapshot master must be array");
                return std::nullopt;
            }
            for (const auto& entry : master) {
                if (!entry.is_object()) {
                    continue;
                }
                MasterWorkspaceSnapshot workspace;
                workspace.workspace_id = entry.value("workspace_id", 0);
                if (entry.contains("orientation")) {
                    const auto parsed_orientation = orientation_from_string(entry.at("orientation"));
                    if (!parsed_orientation.has_value()) {
                        set_error(error, "layout snapshot orientation is invalid");
                        return std::nullopt;
                    }
                    workspace.orientation = *parsed_orientation;
                }
                const auto nodes = entry.value("nodes", nlohmann::json::array());
                if (!nodes.is_array()) {
                    set_error(error, "layout snapshot nodes must be array");
                    return std::nullopt;
                }
                for (const auto& node_entry : nodes) {
                    if (!node_entry.is_object()) {
                        continue;
                    }
                    MasterNodeSnapshot node;
                    if (node_entry.contains("toplevel") && node_entry.at("toplevel").is_string()) {
                        node.toplevel = node_entry.at("toplevel").get<std::string>();
                    }
                    if (node_entry.contains("is_master")) {
                        if (!node_entry.at("is_master").is_boolean()) {
                            set_error(error, "layout snapshot node is_master must be boolean");
                            return std::nullopt;
                        }
                        node.is_master = node_entry.at("is_master").get<bool>();
                    }
                    if (node_entry.contains("perc_master")) {
                        if (!node_entry.at("perc_master").is_number()) {
                            set_error(error, "layout snapshot node perc_master must be number");
                            return std::nullopt;
                        }
                        node.perc_master = node_entry.at("perc_master").get<float>();
                    }
                    if (node_entry.contains("perc_size")) {
                        if (!node_entry.at("perc_size").is_number()) {
                            set_error(error, "layout snapshot node perc_size must be number");
                            return std::nullopt;
                        }
                        node.perc_size = node_entry.at("perc_size").get<float>();
                    }
                    if (node_entry.contains("index")) {
                        if (!node_entry.at("index").is_number_integer()) {
                            set_error(error, "layout snapshot node index must be integer");
                            return std::nullopt;
                        }
                        node.index = node_entry.at("index").get<int>();
                    }
                    workspace.nodes.push_back(std::move(node));
                }
                snapshot.master.push_back(std::move(workspace));
            }
            return snapshot;
        } catch (const std::exception& exc) {
            set_error(error, std::string("layout snapshot decode failed: ") + exc.what());
            return std::nullopt;
        }
    }

} // namespace hyprspaces
