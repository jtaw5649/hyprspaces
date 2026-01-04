#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace hyprspaces {

    std::optional<std::string> optional_string(const nlohmann::json& value);
    std::optional<std::string> optional_string_field(const nlohmann::json& obj, const char* key);
    std::optional<int>         optional_int_field(const nlohmann::json& obj, const char* key);

}
