#include "hyprspaces/dispatch_result.hpp"

namespace hyprspaces {

    DispatchResult dispatch_result_from_output(const CommandOutput& output) {
        if (output.success) {
            return DispatchResult{.pass_event = false, .success = true, .error = ""};
        }
        return DispatchResult{.pass_event = false, .success = false, .error = output.output};
    }

} // namespace hyprspaces
