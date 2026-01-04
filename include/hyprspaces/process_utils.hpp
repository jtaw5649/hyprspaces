#ifndef HYPRSPACES_PROCESS_UTILS_HPP
#define HYPRSPACES_PROCESS_UTILS_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "hyprspaces/types.hpp"

namespace hyprspaces {

    std::optional<std::string> read_process_cmdline(int pid, const std::filesystem::path& proc_root);
    void                       fill_client_commands(std::vector<ClientInfo>& clients, const std::filesystem::path& proc_root);

} // namespace hyprspaces

#endif // HYPRSPACES_PROCESS_UTILS_HPP
