#pragma once

#include <string>
#include <string_view>

namespace hyprspaces {

    std::string_view trim_view(std::string_view value);
    std::string      trim_copy(std::string_view value);

}
