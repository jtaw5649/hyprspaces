#ifndef HYPRSPACES_DISPATCH_RESULT_HPP
#define HYPRSPACES_DISPATCH_RESULT_HPP

#include <string>

#include "hyprspaces/runtime.hpp"

namespace hyprspaces {

    struct DispatchResult {
        bool        pass_event;
        bool        success;
        std::string error;
    };

    DispatchResult dispatch_result_from_output(const CommandOutput& output);

} // namespace hyprspaces

#endif // HYPRSPACES_DISPATCH_RESULT_HPP
