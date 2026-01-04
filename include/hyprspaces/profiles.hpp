#ifndef HYPRSPACES_PROFILES_HPP
#define HYPRSPACES_PROFILES_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hyprspaces {

    struct ProfileWorkspaceSpec {
        std::string                profile;
        int                        workspace_id = 0;
        std::optional<std::string> name         = std::nullopt;
        std::optional<std::string> layout       = std::nullopt;
    };

    struct ProfileWindowSpec {
        std::string                profile;
        std::optional<std::string> app_id        = std::nullopt;
        std::optional<std::string> initial_class = std::nullopt;
        std::optional<std::string> title         = std::nullopt;
        std::optional<std::string> command       = std::nullopt;
        std::optional<int>         workspace_id  = std::nullopt;
        std::optional<bool>        floating      = std::nullopt;
    };

    struct ProfileDefinition {
        std::string                       name;
        std::optional<std::string>        description = std::nullopt;
        std::vector<ProfileWorkspaceSpec> workspaces;
        std::vector<ProfileWindowSpec>    windows;
    };

    class ProfileCatalog {
      public:
        void                                  clear();
        void                                  set_active_profile(std::string name);
        std::optional<std::string_view>       active_profile() const;

        ProfileDefinition&                    ensure_profile(std::string_view name);
        void                                  set_description(std::string_view name, std::string description);
        void                                  add_workspace(ProfileWorkspaceSpec spec);
        void                                  add_window(ProfileWindowSpec spec);
        void                                  upsert_profile(ProfileDefinition profile);

        const ProfileDefinition*              find(std::string_view name) const;
        const std::vector<ProfileDefinition>& profiles() const;
        std::vector<std::string>              list_names() const;

      private:
        std::vector<ProfileDefinition>          profiles_;
        std::unordered_map<std::string, size_t> index_by_name_;
        std::optional<std::string>              active_profile_;
    };

    std::string              parse_profile_name(std::string_view value);
    std::string              parse_profile_description(std::string_view value);
    ProfileWorkspaceSpec     parse_profile_workspace(std::string_view value, std::optional<std::string> active_profile);
    ProfileWindowSpec        parse_profile_window(std::string_view value, std::optional<std::string> active_profile);
    std::vector<std::string> render_profile_lines(const ProfileDefinition& profile);
    std::string              render_profile_text(const ProfileDefinition& profile);

} // namespace hyprspaces

#endif // HYPRSPACES_PROFILES_HPP
