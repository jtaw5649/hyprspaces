#include "hyprspaces/session_protocol_store.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace hyprspaces {

    namespace {

        constexpr int              kStoreVersion = 3;

        std::optional<std::string> read_text_file(const std::filesystem::path& path) {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec) || ec) {
                return std::nullopt;
            }
            std::ifstream input(path);
            if (!input.good()) {
                return std::nullopt;
            }
            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }

        auto find_toplevel(std::vector<SessionProtocolStore::ToplevelState>& toplevels, std::string_view name) {
            return std::ranges::find_if(toplevels, [&](const SessionProtocolStore::ToplevelState& entry) { return entry.name == name; });
        }

    } // namespace

    std::optional<SessionProtocolStore> SessionProtocolStore::load(const std::filesystem::path& path, std::string* error) {
        if (error) {
            error->clear();
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            if (ec) {
                if (error) {
                    *error = "failed to read session store";
                }
                return std::nullopt;
            }
            return SessionProtocolStore{};
        }

        const auto contents = read_text_file(path);
        if (!contents) {
            if (error) {
                *error = "failed to read session store";
            }
            return std::nullopt;
        }

        auto root = nlohmann::json::parse(*contents, nullptr, false);
        if (root.is_discarded()) {
            if (error) {
                *error = "invalid session store";
            }
            return std::nullopt;
        }

        if (!root.is_object()) {
            if (error) {
                *error = "invalid session store";
            }
            return std::nullopt;
        }

        const int version = root.value("version", kStoreVersion);
        if (version != kStoreVersion) {
            if (error) {
                *error = "unsupported session store version";
            }
            return std::nullopt;
        }

        const auto sessions = root.value("sessions", nlohmann::json::array());
        if (!sessions.is_array()) {
            if (error) {
                *error = "invalid session store";
            }
            return std::nullopt;
        }

        SessionProtocolStore store;
        for (const auto& session : sessions) {
            if (!session.is_object()) {
                if (error) {
                    *error = "invalid session store";
                }
                return std::nullopt;
            }
            const auto id = session.value("id", std::string{});
            if (id.empty()) {
                if (error) {
                    *error = "invalid session store";
                }
                return std::nullopt;
            }
            store.remember_session(id);
            const auto toplevels = session.value("toplevels", nlohmann::json::array());
            if (!toplevels.is_array()) {
                if (error) {
                    *error = "invalid session store";
                }
                return std::nullopt;
            }
            for (const auto& entry : toplevels) {
                if (!entry.is_object()) {
                    if (error) {
                        *error = "invalid session store";
                    }
                    return std::nullopt;
                }
                const auto name = entry.value("name", std::string{});
                if (name.empty()) {
                    if (error) {
                        *error = "invalid session store";
                    }
                    return std::nullopt;
                }
                store.add_toplevel(id, name);

                SessionProtocolStore::ToplevelState state{.name = name};
                if (entry.contains("geometry")) {
                    const auto& geometry = entry.at("geometry");
                    if (!geometry.is_object() || !geometry.contains("x") || !geometry.contains("y") || !geometry.contains("width") || !geometry.contains("height") ||
                        !geometry.at("x").is_number_integer() || !geometry.at("y").is_number_integer() || !geometry.at("width").is_number_integer() ||
                        !geometry.at("height").is_number_integer()) {
                        if (error) {
                            *error = "invalid session store";
                        }
                        return std::nullopt;
                    }
                    state.geometry = WindowGeometry{
                        .x      = geometry.at("x").get<int>(),
                        .y      = geometry.at("y").get<int>(),
                        .width  = geometry.at("width").get<int>(),
                        .height = geometry.at("height").get<int>(),
                    };
                }
                if (entry.contains("floating")) {
                    if (!entry.at("floating").is_boolean()) {
                        if (error) {
                            *error = "invalid session store";
                        }
                        return std::nullopt;
                    }
                    state.floating = entry.at("floating").get<bool>();
                }
                if (entry.contains("pinned")) {
                    if (!entry.at("pinned").is_boolean()) {
                        if (error) {
                            *error = "invalid session store";
                        }
                        return std::nullopt;
                    }
                    state.pinned = entry.at("pinned").get<bool>();
                }
                if (entry.contains("fullscreen")) {
                    const auto& fullscreen = entry.at("fullscreen");
                    if (!fullscreen.is_object() || !fullscreen.contains("internal") || !fullscreen.contains("client") || !fullscreen.at("internal").is_number_integer() ||
                        !fullscreen.at("client").is_number_integer()) {
                        if (error) {
                            *error = "invalid session store";
                        }
                        return std::nullopt;
                    }
                    state.fullscreen = FullscreenState{
                        .internal = fullscreen.at("internal").get<int>(),
                        .client   = fullscreen.at("client").get<int>(),
                    };
                }
                store.update_toplevel_state(id, name, state);
            }
            if (session.contains("layout")) {
                if (session.at("layout").is_null()) {
                    continue;
                }
                if (!session.at("layout").is_object()) {
                    if (error) {
                        *error = "invalid session store";
                    }
                    return std::nullopt;
                }
                std::string layout_error;
                const auto  layout = layout_snapshot_from_json(session.at("layout").dump(), &layout_error);
                if (!layout) {
                    if (error) {
                        *error = layout_error.empty() ? "invalid session store" : layout_error;
                    }
                    return std::nullopt;
                }
                store.update_layout_snapshot(id, *layout);
            }
        }

        return store;
    }

    bool SessionProtocolStore::save(const std::filesystem::path& path, std::string* error) const {
        if (error) {
            error->clear();
        }
        const auto parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                if (error) {
                    *error = "failed to create session store directory";
                }
                return false;
            }
        }

        nlohmann::json root;
        root["version"]  = kStoreVersion;
        root["sessions"] = nlohmann::json::array();
        for (const auto& [id, session] : sessions_) {
            nlohmann::json entry;
            entry["id"]              = id;
            nlohmann::json toplevels = nlohmann::json::array();
            for (const auto& toplevel : session.toplevels) {
                nlohmann::json toplevel_entry;
                toplevel_entry["name"] = toplevel.name;
                if (toplevel.geometry) {
                    toplevel_entry["geometry"] = {
                        {"x", toplevel.geometry->x},
                        {"y", toplevel.geometry->y},
                        {"width", toplevel.geometry->width},
                        {"height", toplevel.geometry->height},
                    };
                }
                if (toplevel.floating) {
                    toplevel_entry["floating"] = *toplevel.floating;
                }
                if (toplevel.pinned) {
                    toplevel_entry["pinned"] = *toplevel.pinned;
                }
                if (toplevel.fullscreen) {
                    toplevel_entry["fullscreen"] = {
                        {"internal", toplevel.fullscreen->internal},
                        {"client", toplevel.fullscreen->client},
                    };
                }
                toplevels.push_back(toplevel_entry);
            }
            entry["toplevels"] = std::move(toplevels);
            if (session.layout) {
                std::string layout_error;
                const auto  layout_json = layout_snapshot_to_json(*session.layout, &layout_error);
                if (!layout_json) {
                    if (error) {
                        *error = layout_error.empty() ? "failed to encode layout snapshot" : layout_error;
                    }
                    return false;
                }
                try {
                    entry["layout"] = nlohmann::json::parse(layout_json->json);
                } catch (const std::exception& ex) {
                    if (error) {
                        *error = std::string("failed to encode layout snapshot: ") + ex.what();
                    }
                    return false;
                }
            }
            root["sessions"].push_back(entry);
        }

        std::ofstream output(path);
        if (!output.good()) {
            if (error) {
                *error = "failed to write session store";
            }
            return false;
        }
        output << root.dump(2);
        output.flush();
        if (!output.good()) {
            if (error) {
                *error = "failed to write session store";
            }
            return false;
        }
        return true;
    }

    bool SessionProtocolStore::remember_session(std::string id) {
        if (id.empty()) {
            return false;
        }
        if (sessions_.contains(id)) {
            return false;
        }
        sessions_.emplace(id, SessionRecord{.id = id, .toplevels = {}});
        return true;
    }

    bool SessionProtocolStore::remove_session(std::string_view id) {
        if (id.empty()) {
            return false;
        }
        return sessions_.erase(std::string(id)) > 0;
    }

    bool SessionProtocolStore::add_toplevel(std::string_view session_id, std::string_view name) {
        if (session_id.empty() || name.empty()) {
            return false;
        }
        const auto it = sessions_.find(std::string(session_id));
        if (it == sessions_.end()) {
            return false;
        }
        auto& toplevels = it->second.toplevels;
        if (find_toplevel(toplevels, name) != toplevels.end()) {
            return false;
        }
        toplevels.push_back(ToplevelState{.name = std::string(name)});
        return true;
    }

    bool SessionProtocolStore::remove_toplevel(std::string_view session_id, std::string_view name) {
        if (session_id.empty() || name.empty()) {
            return false;
        }
        const auto it = sessions_.find(std::string(session_id));
        if (it == sessions_.end()) {
            return false;
        }
        auto&      toplevels = it->second.toplevels;
        const auto found     = find_toplevel(toplevels, name);
        if (found == toplevels.end()) {
            return false;
        }
        toplevels.erase(found);
        return true;
    }

    bool SessionProtocolStore::update_toplevel_state(std::string_view session_id, std::string_view name, const ToplevelState& state) {
        if (session_id.empty() || name.empty()) {
            return false;
        }
        const auto it = sessions_.find(std::string(session_id));
        if (it == sessions_.end()) {
            return false;
        }
        auto& toplevels = it->second.toplevels;
        auto  found     = find_toplevel(toplevels, name);
        if (found == toplevels.end()) {
            return false;
        }
        found->name       = std::string(name);
        found->geometry   = state.geometry;
        found->floating   = state.floating;
        found->pinned     = state.pinned;
        found->fullscreen = state.fullscreen;
        return true;
    }

    bool SessionProtocolStore::update_layout_snapshot(std::string_view session_id, const LayoutSnapshot& snapshot) {
        if (session_id.empty()) {
            return false;
        }
        const auto it = sessions_.find(std::string(session_id));
        if (it == sessions_.end()) {
            return false;
        }
        it->second.layout = snapshot;
        return true;
    }

    bool SessionProtocolStore::has_session(std::string_view id) const {
        if (id.empty()) {
            return false;
        }
        return sessions_.contains(std::string(id));
    }

    std::vector<std::string> SessionProtocolStore::session_ids() const {
        std::vector<std::string> ids;
        ids.reserve(sessions_.size());
        for (const auto& [id, session] : sessions_) {
            ids.push_back(id);
        }
        std::ranges::sort(ids);
        return ids;
    }

    std::optional<std::vector<SessionProtocolStore::ToplevelState>> SessionProtocolStore::toplevels_for(std::string_view id) const {
        if (id.empty()) {
            return std::nullopt;
        }
        const auto it = sessions_.find(std::string(id));
        if (it == sessions_.end()) {
            return std::nullopt;
        }
        return it->second.toplevels;
    }

    std::optional<LayoutSnapshot> SessionProtocolStore::layout_for(std::string_view id) const {
        if (id.empty()) {
            return std::nullopt;
        }
        const auto it = sessions_.find(std::string(id));
        if (it == sessions_.end()) {
            return std::nullopt;
        }
        return it->second.layout;
    }

} // namespace hyprspaces
