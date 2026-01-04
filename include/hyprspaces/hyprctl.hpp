#ifndef HYPRSPACES_HYPRCTL_HPP
#define HYPRSPACES_HYPRCTL_HPP

#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "hyprspaces/dispatch.hpp"
#include "hyprspaces/types.hpp"

namespace hyprspaces {

    class HyprctlError : public std::runtime_error {
      public:
        using std::runtime_error::runtime_error;
    };

    struct HyprctlErrorInfo {
        std::string context;
        std::string message;
    };

    inline std::string format_hyprctl_error(const HyprctlErrorInfo& error) {
        std::string text = error.context;
        if (!text.empty() && !error.message.empty()) {
            text.append(": ");
        }
        text.append(error.message);
        return text;
    }

    template <typename T>
    using HyprctlResult = std::expected<T, HyprctlErrorInfo>;

    class HyprctlInvoker {
      public:
        virtual ~HyprctlInvoker()                                                                         = default;
        virtual std::string invoke(std::string_view call, std::string_view args, std::string_view format) = 0;
    };

    HyprctlResult<int>                        parse_active_workspace_id(std::string_view json_text);
    HyprctlResult<std::string>                parse_active_window_address(std::string_view json_text);
    HyprctlResult<std::vector<MonitorInfo>>   parse_monitors(std::string_view json_text);
    HyprctlResult<std::vector<WorkspaceInfo>> parse_workspaces(std::string_view json_text);
    HyprctlResult<std::vector<ClientInfo>>    parse_clients(std::string_view json_text);

    class HyprctlClient {
      public:
        explicit HyprctlClient(HyprctlInvoker& invoker);

        HyprctlResult<int>                        active_workspace_id();
        HyprctlResult<std::string>                active_window_address();
        HyprctlResult<std::vector<MonitorInfo>>   monitors();
        HyprctlResult<std::vector<WorkspaceInfo>> workspaces();
        HyprctlResult<std::vector<ClientInfo>>    clients();

        std::string                               dispatch(const std::string& dispatcher, const std::string& argument);
        std::string                               dispatch_batch(const std::vector<DispatchCommand>& commands);
        std::string                               keyword(const std::string& name, const std::string& value);
        std::string                               reload();

      private:
        HyprctlInvoker& invoker_;
    };

    bool is_ok_response(std::string_view output);

} // namespace hyprspaces

#endif // HYPRSPACES_HYPRCTL_HPP
