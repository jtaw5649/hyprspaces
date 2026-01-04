#include "hyprspaces/config_value.hpp"

#include <cstdint>

#include "hyprspaces/config.hpp"

namespace hyprspaces {

    namespace {

        std::optional<int64_t> any_to_int64(const std::any& value) {
            if (const auto* raw = std::any_cast<int64_t>(&value)) {
                return *raw;
            }
            if (const auto* raw = std::any_cast<int>(&value)) {
                return static_cast<int64_t>(*raw);
            }
            return std::nullopt;
        }

        std::optional<bool> any_to_bool(const std::any& value) {
            if (const auto* raw = std::any_cast<bool>(&value)) {
                return *raw;
            }
            if (const auto as_int = any_to_int64(value)) {
                return *as_int != 0;
            }
            return std::nullopt;
        }

        std::optional<std::string> any_to_string(const std::any& value) {
            if (const auto* raw = std::any_cast<const char*>(&value)) {
                if (!raw || !*raw) {
                    return std::string{};
                }
                return std::string(*raw);
            }
            if (const auto* raw = std::any_cast<std::string>(&value)) {
                return *raw;
            }
            return std::nullopt;
        }

    } // namespace

    std::optional<std::string> read_string_override(const ConfigValueView& view) {
        if (!view.set_by_user) {
            return std::nullopt;
        }
        const auto raw = any_to_string(view.value);
        if (!raw) {
            return std::nullopt;
        }
        return normalize_override_string(*raw);
    }

    std::optional<int> read_positive_int_override(const ConfigValueView& view) {
        if (!view.set_by_user) {
            return std::nullopt;
        }
        const auto raw = any_to_int64(view.value);
        if (!raw) {
            return std::nullopt;
        }
        const int parsed = static_cast<int>(*raw);
        if (parsed <= 0) {
            return std::nullopt;
        }
        return parsed;
    }

    std::optional<int> read_non_negative_int_override(const ConfigValueView& view) {
        if (!view.set_by_user) {
            return std::nullopt;
        }
        const auto raw = any_to_int64(view.value);
        if (!raw) {
            return std::nullopt;
        }
        const int parsed = static_cast<int>(*raw);
        if (parsed < 0) {
            return std::nullopt;
        }
        return parsed;
    }

    std::optional<bool> read_bool_override(const ConfigValueView& view) {
        if (!view.set_by_user) {
            return std::nullopt;
        }
        return any_to_bool(view.value);
    }

    bool read_bool_value(const ConfigValueView& view, bool default_value) {
        if (const auto raw = any_to_bool(view.value)) {
            return *raw;
        }
        return default_value;
    }

} // namespace hyprspaces
