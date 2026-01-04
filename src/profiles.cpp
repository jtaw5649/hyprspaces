#include "hyprspaces/profiles.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "hyprspaces/strings.hpp"

namespace hyprspaces {

    namespace {

        std::vector<std::string> split_tokens(std::string_view value, char delim) {
            std::vector<std::string> tokens;
            size_t                   start = 0;
            while (start <= value.size()) {
                const auto end  = value.find(delim, start);
                const auto part = end == std::string_view::npos ? value.substr(start) : value.substr(start, end - start);
                tokens.emplace_back(trim_copy(part));
                if (end == std::string_view::npos) {
                    break;
                }
                start = end + 1;
            }
            return tokens;
        }

        int parse_positive_int(std::string_view value, std::string_view label) {
            const auto trimmed = trim_copy(value);
            if (trimmed.empty()) {
                throw std::runtime_error(std::string(label) + " value is empty");
            }
            try {
                const int parsed = std::stoi(trimmed);
                if (parsed <= 0) {
                    throw std::runtime_error(std::string(label) + " must be positive");
                }
                return parsed;
            } catch (const std::exception&) { throw std::runtime_error(std::string("invalid ") + std::string(label)); }
        }

        std::optional<bool> parse_bool(std::string_view value) {
            auto trimmed = trim_copy(value);
            if (trimmed.empty()) {
                return std::nullopt;
            }
            std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (trimmed == "true" || trimmed == "1") {
                return true;
            }
            if (trimmed == "false" || trimmed == "0") {
                return false;
            }
            return std::nullopt;
        }

        std::pair<std::string, std::string> split_key_value(std::string_view token) {
            const auto colon = token.find(':');
            if (colon == std::string_view::npos) {
                throw std::runtime_error("profile entry missing ':'");
            }
            const auto key   = trim_copy(token.substr(0, colon));
            const auto value = trim_copy(token.substr(colon + 1));
            if (key.empty() || value.empty()) {
                throw std::runtime_error("profile entry missing key/value");
            }
            return {key, value};
        }

        std::string require_profile(std::optional<std::string> active_profile, const std::string& inline_profile) {
            if (!inline_profile.empty()) {
                return inline_profile;
            }
            if (active_profile && !active_profile->empty()) {
                return *active_profile;
            }
            throw std::runtime_error("profile name is required");
        }

    } // namespace

    std::string parse_profile_name(std::string_view value) {
        const auto name = trim_copy(value);
        if (name.empty()) {
            throw std::runtime_error("profile name is required");
        }
        return name;
    }

    std::string parse_profile_description(std::string_view value) {
        const auto description = trim_copy(value);
        if (description.empty()) {
            throw std::runtime_error("profile description is required");
        }
        return description;
    }

    ProfileWorkspaceSpec parse_profile_workspace(std::string_view value, std::optional<std::string> active_profile) {
        ProfileWorkspaceSpec spec;
        std::string          inline_profile;

        const auto           tokens = split_tokens(value, ',');
        for (const auto& raw : tokens) {
            if (raw.empty()) {
                throw std::runtime_error("profile workspace entry is empty");
            }
            if (raw.find(':') == std::string_view::npos) {
                spec.workspace_id = parse_positive_int(raw, "workspace id");
                continue;
            }
            const auto [key, val] = split_key_value(raw);
            if (key == "profile") {
                inline_profile = val;
                continue;
            }
            if (key == "id") {
                spec.workspace_id = parse_positive_int(val, "workspace id");
                continue;
            }
            if (key == "name") {
                spec.name = val;
                continue;
            }
            if (key == "layout") {
                spec.layout = val;
                continue;
            }
            throw std::runtime_error("unknown profile workspace field");
        }

        spec.profile = require_profile(std::move(active_profile), inline_profile);
        if (spec.workspace_id <= 0) {
            throw std::runtime_error("workspace id is required");
        }
        return spec;
    }

    ProfileWindowSpec parse_profile_window(std::string_view value, std::optional<std::string> active_profile) {
        ProfileWindowSpec spec;
        std::string       inline_profile;

        const auto        tokens = split_tokens(value, ',');
        for (const auto& raw : tokens) {
            if (raw.empty()) {
                throw std::runtime_error("profile window entry is empty");
            }
            const auto [key, val] = split_key_value(raw);
            if (key == "profile") {
                inline_profile = val;
                continue;
            }
            if (key == "app_id") {
                spec.app_id = val;
                continue;
            }
            if (key == "class" || key == "initial_class") {
                spec.initial_class = val;
                continue;
            }
            if (key == "title") {
                spec.title = val;
                continue;
            }
            if (key == "command") {
                spec.command = val;
                continue;
            }
            if (key == "workspace") {
                spec.workspace_id = parse_positive_int(val, "workspace");
                continue;
            }
            if (key == "floating") {
                const auto parsed = parse_bool(val);
                if (!parsed) {
                    throw std::runtime_error("invalid floating value");
                }
                spec.floating = *parsed;
                continue;
            }
            throw std::runtime_error("unknown profile window field");
        }

        spec.profile = require_profile(std::move(active_profile), inline_profile);
        if (!spec.app_id && !spec.initial_class && !spec.title && !spec.command) {
            throw std::runtime_error("profile window requires an identifier");
        }
        return spec;
    }

    void ProfileCatalog::clear() {
        profiles_.clear();
        index_by_name_.clear();
        active_profile_.reset();
    }

    void ProfileCatalog::set_active_profile(std::string name) {
        if (name.empty()) {
            active_profile_.reset();
            return;
        }
        ensure_profile(name);
        active_profile_ = std::move(name);
    }

    std::optional<std::string_view> ProfileCatalog::active_profile() const {
        if (!active_profile_ || active_profile_->empty()) {
            return std::nullopt;
        }
        return *active_profile_;
    }

    ProfileDefinition& ProfileCatalog::ensure_profile(std::string_view name) {
        const auto key = std::string(name);
        if (auto it = index_by_name_.find(key); it != index_by_name_.end()) {
            return profiles_[it->second];
        }
        const size_t index = profiles_.size();
        profiles_.push_back(ProfileDefinition{
            .name        = key,
            .description = std::nullopt,
            .workspaces  = {},
            .windows     = {},
        });
        index_by_name_.emplace(key, index);
        return profiles_[index];
    }

    void ProfileCatalog::set_description(std::string_view name, std::string description) {
        if (name.empty()) {
            return;
        }
        auto& profile       = ensure_profile(name);
        profile.description = std::move(description);
    }

    void ProfileCatalog::add_workspace(ProfileWorkspaceSpec spec) {
        if (spec.profile.empty()) {
            return;
        }
        auto& profile = ensure_profile(spec.profile);
        profile.workspaces.push_back(std::move(spec));
    }

    void ProfileCatalog::add_window(ProfileWindowSpec spec) {
        if (spec.profile.empty()) {
            return;
        }
        auto& profile = ensure_profile(spec.profile);
        profile.windows.push_back(std::move(spec));
    }

    void ProfileCatalog::upsert_profile(ProfileDefinition profile) {
        if (profile.name.empty()) {
            return;
        }
        const auto key = profile.name;
        if (const auto it = index_by_name_.find(key); it != index_by_name_.end()) {
            profiles_[it->second] = std::move(profile);
            return;
        }
        const size_t index = profiles_.size();
        profiles_.push_back(std::move(profile));
        index_by_name_.emplace(key, index);
    }

    const ProfileDefinition* ProfileCatalog::find(std::string_view name) const {
        if (name.empty()) {
            return nullptr;
        }
        const auto it = index_by_name_.find(std::string(name));
        if (it == index_by_name_.end()) {
            return nullptr;
        }
        return &profiles_[it->second];
    }

    const std::vector<ProfileDefinition>& ProfileCatalog::profiles() const {
        return profiles_;
    }

    std::vector<std::string> ProfileCatalog::list_names() const {
        std::vector<std::string> names;
        names.reserve(profiles_.size());
        for (const auto& profile : profiles_) {
            names.push_back(profile.name);
        }
        return names;
    }

    namespace {

        std::string format_bool(bool value) {
            return value ? "true" : "false";
        }

        std::string format_profile_workspace(const ProfileWorkspaceSpec& spec, std::string_view base_profile) {
            std::vector<std::string> parts;
            if (spec.profile != base_profile) {
                parts.push_back("profile:" + spec.profile);
            }
            parts.push_back(std::to_string(spec.workspace_id));
            if (spec.name && !spec.name->empty()) {
                parts.push_back("name:" + *spec.name);
            }
            if (spec.layout && !spec.layout->empty()) {
                parts.push_back("layout:" + *spec.layout);
            }
            std::string output;
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i != 0) {
                    output.append(", ");
                }
                output.append(parts[i]);
            }
            return output;
        }

        std::string format_profile_window(const ProfileWindowSpec& spec, std::string_view base_profile) {
            std::vector<std::string> parts;
            if (spec.profile != base_profile) {
                parts.push_back("profile:" + spec.profile);
            }
            if (spec.app_id && !spec.app_id->empty()) {
                parts.push_back("app_id:" + *spec.app_id);
            }
            if (spec.initial_class && !spec.initial_class->empty()) {
                parts.push_back("class:" + *spec.initial_class);
            }
            if (spec.title && !spec.title->empty()) {
                parts.push_back("title:" + *spec.title);
            }
            if (spec.command && !spec.command->empty()) {
                parts.push_back("command:" + *spec.command);
            }
            if (spec.workspace_id) {
                parts.push_back("workspace:" + std::to_string(*spec.workspace_id));
            }
            if (spec.floating) {
                parts.push_back("floating:" + format_bool(*spec.floating));
            }
            std::string output;
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i != 0) {
                    output.append(", ");
                }
                output.append(parts[i]);
            }
            return output;
        }

    } // namespace

    std::vector<std::string> render_profile_lines(const ProfileDefinition& profile) {
        std::vector<std::string> lines;
        lines.reserve(2 + profile.workspaces.size() + profile.windows.size());
        lines.push_back("plugin:hyprspaces:profile = " + profile.name);
        if (profile.description && !profile.description->empty()) {
            lines.push_back("plugin:hyprspaces:profile_description = " + *profile.description);
        }
        for (const auto& workspace : profile.workspaces) {
            lines.push_back("plugin:hyprspaces:profile_workspace = " + format_profile_workspace(workspace, profile.name));
        }
        for (const auto& window : profile.windows) {
            lines.push_back("plugin:hyprspaces:profile_window = " + format_profile_window(window, profile.name));
        }
        return lines;
    }

    std::string render_profile_text(const ProfileDefinition& profile) {
        const auto  lines = render_profile_lines(profile);
        std::string output;
        for (const auto& line : lines) {
            output.append(line);
            output.push_back('\n');
        }
        return output;
    }

} // namespace hyprspaces
