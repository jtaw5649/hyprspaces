#include "hyprspaces/strings.hpp"

#include <cctype>

namespace hyprspaces {

    std::string_view trim_view(std::string_view value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
            ++start;
        }
        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }
        return value.substr(start, end - start);
    }

    std::string trim_copy(std::string_view value) {
        return std::string(trim_view(value));
    }

}
