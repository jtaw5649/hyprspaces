#include "hyprspaces/json_utils.hpp"

namespace hyprspaces {

    std::optional<std::string> optional_string(const nlohmann::json& value) {
        if (value.is_null()) {
            return std::nullopt;
        }
        if (!value.is_string()) {
            return std::nullopt;
        }
        auto str = value.get<std::string>();
        if (str.empty()) {
            return std::nullopt;
        }
        return str;
    }

    std::optional<std::string> optional_string_field(const nlohmann::json& obj, const char* key) {
        if (!obj.contains(key)) {
            return std::nullopt;
        }
        return optional_string(obj.at(key));
    }

    std::optional<int> optional_int_field(const nlohmann::json& obj, const char* key) {
        if (!obj.contains(key) || obj.at(key).is_null()) {
            return std::nullopt;
        }
        return obj.at(key).get<int>();
    }

}
