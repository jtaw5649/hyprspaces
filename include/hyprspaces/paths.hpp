#ifndef HYPRSPACES_PATHS_HPP
#define HYPRSPACES_PATHS_HPP

#include <filesystem>
#include <optional>
#include <string>

namespace hyprspaces {

    struct EnvConfig {
        std::optional<std::string> home;
        std::optional<std::string> xdg_config_home;
        std::optional<std::string> xdg_state_home;
    };

    struct Paths {
        std::filesystem::path base_dir;
        std::filesystem::path hyprspaces_conf_path;
        std::filesystem::path hyprland_conf_path;
        std::filesystem::path profiles_dir;
        std::filesystem::path proc_root;
        std::filesystem::path state_dir;
        std::filesystem::path session_store_path;
        std::filesystem::path waybar_dir;
        std::filesystem::path waybar_theme_css;
    };

    Paths                resolve_paths(const EnvConfig& env);
    Paths                resolve_paths_from_env();
    std::optional<Paths> try_resolve_paths(const EnvConfig& env);
    std::optional<Paths> try_resolve_paths_from_env();
} // namespace hyprspaces

#endif // HYPRSPACES_PATHS_HPP
