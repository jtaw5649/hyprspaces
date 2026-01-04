#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hyprspaces {

    struct FunctionMatch {
        void*       address = nullptr;
        std::string signature;
        std::string demangled;
    };

    std::optional<FunctionMatch> select_function_match(const std::vector<FunctionMatch>& matches, std::string_view class_tag, std::string_view signature_hint);

} // namespace hyprspaces
