#include "hyprspaces/hyprctl.hpp"

#include "hyprspaces/json_utils.hpp"
#include "hyprspaces/strings.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace hyprspaces {

    namespace {

        HyprctlResult<nlohmann::json> parse_json(std::string_view json_text, std::string_view context) {
            auto parsed = nlohmann::json::parse(json_text.begin(), json_text.end(), nullptr, false);
            if (parsed.is_discarded()) {
                return std::unexpected(HyprctlErrorInfo{std::string(context), "invalid json"});
            }
            return parsed;
        }

        HyprctlResult<std::string> required_string_field(const nlohmann::json& obj, std::string_view key, std::string_view context) {
            if (!obj.contains(key)) {
                return std::unexpected(HyprctlErrorInfo{std::string(context), std::string(key) + " missing"});
            }
            const auto& value = obj.at(key);
            if (!value.is_string()) {
                return std::unexpected(HyprctlErrorInfo{std::string(context), std::string(key) + " invalid"});
            }
            return value.get<std::string>();
        }

        HyprctlResult<int> required_int_field(const nlohmann::json& obj, std::string_view key, std::string_view context) {
            if (!obj.contains(key)) {
                return std::unexpected(HyprctlErrorInfo{std::string(context), std::string(key) + " missing"});
            }
            const auto& value = obj.at(key);
            if (!value.is_number_integer()) {
                return std::unexpected(HyprctlErrorInfo{std::string(context), std::string(key) + " invalid"});
            }
            return value.get<int>();
        }

        HyprctlResult<const nlohmann::json*> required_object_field(const nlohmann::json& obj, std::string_view key, std::string_view context) {
            if (!obj.contains(key)) {
                return std::unexpected(HyprctlErrorInfo{std::string(context), std::string(key) + " missing"});
            }
            const auto& value = obj.at(key);
            if (!value.is_object()) {
                return std::unexpected(HyprctlErrorInfo{std::string(context), std::string(key) + " invalid"});
            }
            return &value;
        }

        std::optional<int> optional_int_field_safe(const nlohmann::json& obj, const char* key) {
            if (!obj.contains(key) || obj.at(key).is_null()) {
                return std::nullopt;
            }
            const auto& value = obj.at(key);
            if (!value.is_number_integer()) {
                return std::nullopt;
            }
            return value.get<int>();
        }

        std::optional<bool> optional_bool_field_safe(const nlohmann::json& obj, const char* key) {
            if (!obj.contains(key) || obj.at(key).is_null()) {
                return std::nullopt;
            }
            const auto& value = obj.at(key);
            if (!value.is_boolean()) {
                return std::nullopt;
            }
            return value.get<bool>();
        }

    } // namespace

    HyprctlResult<int> parse_active_workspace_id(std::string_view json_text) {
        const auto json = parse_json(json_text, "activeworkspace");
        if (!json) {
            return std::unexpected(json.error());
        }
        return required_int_field(*json, "id", "activeworkspace");
    }

    HyprctlResult<std::string> parse_active_window_address(std::string_view json_text) {
        const auto json = parse_json(json_text, "activewindow");
        if (!json) {
            return std::unexpected(json.error());
        }
        return required_string_field(*json, "address", "activewindow");
    }

    HyprctlResult<std::vector<MonitorInfo>> parse_monitors(std::string_view json_text) {
        const auto json = parse_json(json_text, "monitors");
        if (!json) {
            return std::unexpected(json.error());
        }
        if (!json->is_array()) {
            return std::unexpected(HyprctlErrorInfo{"monitors", "not array"});
        }
        std::vector<MonitorInfo> monitors;
        monitors.reserve(json->size());
        for (const auto& monitor : *json) {
            const auto name = required_string_field(monitor, "name", "monitors");
            if (!name) {
                return std::unexpected(name.error());
            }
            const auto id = required_int_field(monitor, "id", "monitors");
            if (!id) {
                return std::unexpected(id.error());
            }
            const auto x = required_int_field(monitor, "x", "monitors");
            if (!x) {
                return std::unexpected(x.error());
            }
            std::optional<int> active_workspace_id;
            if (monitor.contains("activeWorkspace") && monitor.at("activeWorkspace").is_object()) {
                const auto& active = monitor.at("activeWorkspace");
                if (active.contains("id") && active.at("id").is_number_integer()) {
                    active_workspace_id = active.at("id").get<int>();
                }
            }
            monitors.push_back(MonitorInfo{
                .name                = *name,
                .id                  = *id,
                .x                   = *x,
                .description         = optional_string_field(monitor, "description"),
                .active_workspace_id = active_workspace_id,
            });
        }
        return monitors;
    }

    HyprctlResult<std::vector<WorkspaceInfo>> parse_workspaces(std::string_view json_text) {
        const auto json = parse_json(json_text, "workspaces");
        if (!json) {
            return std::unexpected(json.error());
        }
        if (!json->is_array()) {
            return std::unexpected(HyprctlErrorInfo{"workspaces", "not array"});
        }
        std::vector<WorkspaceInfo> workspaces;
        workspaces.reserve(json->size());
        for (const auto& workspace : *json) {
            const auto id = required_int_field(workspace, "id", "workspaces");
            if (!id) {
                return std::unexpected(id.error());
            }
            const auto windows = required_int_field(workspace, "windows", "workspaces");
            if (!windows) {
                return std::unexpected(windows.error());
            }
            workspaces.push_back(WorkspaceInfo{
                .id      = *id,
                .windows = *windows,
                .name    = optional_string_field(workspace, "name"),
                .monitor = optional_string_field(workspace, "monitor"),
            });
        }
        return workspaces;
    }

    HyprctlResult<std::vector<ClientInfo>> parse_clients(std::string_view json_text) {
        const auto json = parse_json(json_text, "clients");
        if (!json) {
            return std::unexpected(json.error());
        }
        if (!json->is_array()) {
            return std::unexpected(HyprctlErrorInfo{"clients", "not array"});
        }
        std::vector<ClientInfo> clients;
        clients.reserve(json->size());
        for (const auto& client : *json) {
            const auto address = required_string_field(client, "address", "clients");
            if (!address) {
                return std::unexpected(address.error());
            }
            const auto workspace = required_object_field(client, "workspace", "clients");
            if (!workspace) {
                return std::unexpected(workspace.error());
            }
            const auto workspace_id = required_int_field(**workspace, "id", "clients");
            if (!workspace_id) {
                return std::unexpected(workspace_id.error());
            }
            const auto                    pid = optional_int_field_safe(client, "pid");

            std::optional<WindowGeometry> geometry;
            if (client.contains("at") && client.contains("size")) {
                const auto& at   = client.at("at");
                const auto& size = client.at("size");
                if (at.is_array() && size.is_array() && at.size() >= 2 && size.size() >= 2) {
                    if (at.at(0).is_number_integer() && at.at(1).is_number_integer() && size.at(0).is_number_integer() && size.at(1).is_number_integer()) {
                        geometry = WindowGeometry{
                            .x      = at.at(0).get<int>(),
                            .y      = at.at(1).get<int>(),
                            .width  = size.at(0).get<int>(),
                            .height = size.at(1).get<int>(),
                        };
                    }
                }
            }

            std::optional<FullscreenState> fullscreen;
            const auto                     fullscreen_internal = optional_int_field_safe(client, "fullscreen");
            const auto                     fullscreen_client   = optional_int_field_safe(client, "fullscreenClient");
            if (fullscreen_internal || fullscreen_client) {
                fullscreen = FullscreenState{
                    .internal = fullscreen_internal.value_or(0),
                    .client   = fullscreen_client.value_or(0),
                };
            }

            clients.push_back(ClientInfo{
                .address       = *address,
                .workspace     = WorkspaceRef{.id = *workspace_id, .name = optional_string_field(**workspace, "name")},
                .class_name    = optional_string_field(client, "class"),
                .title         = optional_string_field(client, "title"),
                .initial_class = optional_string_field(client, "initialClass"),
                .initial_title = optional_string_field(client, "initialTitle"),
                .app_id        = optional_string_field(client, "app_id"),
                .pid           = pid,
                .command       = std::nullopt,
                .geometry      = geometry,
                .floating      = optional_bool_field_safe(client, "floating"),
                .pinned        = optional_bool_field_safe(client, "pinned"),
                .fullscreen    = fullscreen,
                .urgent        = std::nullopt,
            });
        }
        return clients;
    }

    HyprctlClient::HyprctlClient(HyprctlInvoker& invoker) : invoker_(invoker) {}

    HyprctlResult<int> HyprctlClient::active_workspace_id() {
        const auto output = invoker_.invoke("activeworkspace", "", "j");
        return parse_active_workspace_id(output);
    }

    HyprctlResult<std::string> HyprctlClient::active_window_address() {
        const auto output = invoker_.invoke("activewindow", "", "j");
        return parse_active_window_address(output);
    }

    HyprctlResult<std::vector<MonitorInfo>> HyprctlClient::monitors() {
        const auto output = invoker_.invoke("monitors", "", "j");
        return parse_monitors(output);
    }

    HyprctlResult<std::vector<WorkspaceInfo>> HyprctlClient::workspaces() {
        const auto output = invoker_.invoke("workspaces", "", "j");
        return parse_workspaces(output);
    }

    HyprctlResult<std::vector<ClientInfo>> HyprctlClient::clients() {
        const auto output = invoker_.invoke("clients", "", "j");
        return parse_clients(output);
    }

    std::string HyprctlClient::dispatch(const std::string& dispatcher, const std::string& argument) {
        const auto args = argument.empty() ? dispatcher : dispatcher + " " + argument;
        return invoker_.invoke("dispatch", args, "");
    }

    std::string HyprctlClient::dispatch_batch(const std::vector<DispatchCommand>& commands) {
        return invoker_.invoke("[[BATCH]]", hyprspaces::dispatch_batch(commands), "");
    }

    std::string HyprctlClient::keyword(const std::string& name, const std::string& value) {
        if (value.empty()) {
            return invoker_.invoke("keyword", name, "");
        }
        return invoker_.invoke("keyword", name + " " + value, "");
    }

    std::string HyprctlClient::reload() {
        return invoker_.invoke("reload", "", "");
    }

    bool is_ok_response(std::string_view output) {
        if (output.empty()) {
            return false;
        }
        constexpr std::string_view kDelimiter = "\n\n\n";
        size_t                     start      = 0;
        while (start <= output.size()) {
            const auto end   = output.find(kDelimiter, start);
            const auto slice = trim_view(output.substr(start, end == std::string_view::npos ? output.size() - start : end - start));
            if (slice.empty()) {
                if (end == std::string_view::npos) {
                    break;
                }
                return false;
            }
            if (slice != "ok") {
                return false;
            }
            if (end == std::string_view::npos) {
                break;
            }
            start = end + kDelimiter.size();
        }
        return true;
    }

} // namespace hyprspaces
