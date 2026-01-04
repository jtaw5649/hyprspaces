#include "hyprspaces/paths.hpp"

#include <cctype>
#include <cstdlib>
#include <string_view>
#include <stdexcept>

namespace hyprspaces {

    namespace {

        std::optional<std::string> get_env(const char* name) {
            if (const char* value = std::getenv(name)) {
                if (*value != '\0') {
                    return std::string(value);
                }
            }
            return std::nullopt;
        }

        std::filesystem::path config_root(const EnvConfig& env) {
            if (env.xdg_config_home) {
                return std::filesystem::path(*env.xdg_config_home);
            }
            if (env.home) {
                return std::filesystem::path(*env.home) / ".config";
            }
            throw std::runtime_error("missing HOME for config root");
        }

        std::filesystem::path state_root(const EnvConfig& env) {
            if (env.xdg_state_home) {
                return std::filesystem::path(*env.xdg_state_home);
            }
            if (env.home) {
                return std::filesystem::path(*env.home) / ".local" / "state";
            }
            throw std::runtime_error("missing HOME for state root");
        }

    } // namespace

    Paths resolve_paths(const EnvConfig& env) {
        const auto root       = config_root(env);
        const auto base_dir   = root / "hyprspaces";
        const auto hypr_root  = root / "hypr";
        const auto state_base = state_root(env) / "hyprspaces";
        return Paths{
            .base_dir             = base_dir,
            .hyprspaces_conf_path = base_dir / "hyprspaces.conf",
            .hyprland_conf_path   = hypr_root / "hyprland.conf",
            .profiles_dir         = hypr_root / "hyprspaces" / "profiles",
            .proc_root            = "/proc",
            .state_dir            = state_base,
            .session_store_path   = state_base / "session-store.json",
            .waybar_dir           = base_dir / "waybar",
            .waybar_theme_css     = base_dir / "waybar" / "theme.css",
        };
    }

    Paths resolve_paths_from_env() {
        const EnvConfig env{
            .home            = get_env("HOME"),
            .xdg_config_home = get_env("XDG_CONFIG_HOME"),
            .xdg_state_home  = get_env("XDG_STATE_HOME"),
        };
        return resolve_paths(env);
    }

    std::optional<Paths> try_resolve_paths(const EnvConfig& env) {
        if (!env.xdg_config_home && !env.home) {
            return std::nullopt;
        }
        return resolve_paths(env);
    }

    std::optional<Paths> try_resolve_paths_from_env() {
        const EnvConfig env{
            .home            = get_env("HOME"),
            .xdg_config_home = get_env("XDG_CONFIG_HOME"),
            .xdg_state_home  = get_env("XDG_STATE_HOME"),
        };
        return try_resolve_paths(env);
    }

} // namespace hyprspaces
