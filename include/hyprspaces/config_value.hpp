#pragma once

#include <any>
#include <optional>
#include <string>

namespace hyprspaces {

    struct ConfigValueView {
        bool     set_by_user = false;
        std::any value;
    };

    std::optional<std::string> read_string_override(const ConfigValueView& view);
    std::optional<int>         read_positive_int_override(const ConfigValueView& view);
    std::optional<int>         read_non_negative_int_override(const ConfigValueView& view);
    std::optional<bool>        read_bool_override(const ConfigValueView& view);
    bool                       read_bool_value(const ConfigValueView& view, bool default_value);

} // namespace hyprspaces
