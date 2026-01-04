#ifndef HYPRSPACES_REGISTRATION_HPP
#define HYPRSPACES_REGISTRATION_HPP

#include <string>
#include <variant>
#include <vector>

namespace hyprspaces {

    enum class ConfigValueType {
        kString,
        kInt,
        kBool,
    };

    struct ConfigValueSpec {
        std::string                          name;
        ConfigValueType                      type;
        std::variant<std::string, int, bool> default_value;
    };

    std::vector<ConfigValueSpec> config_specs();

} // namespace hyprspaces

#endif // HYPRSPACES_REGISTRATION_HPP
