#include "hyprspaces/process_utils.hpp"

#include <fstream>

namespace hyprspaces {

    std::optional<std::string> read_process_cmdline(int pid, const std::filesystem::path& proc_root) {
        if (pid <= 0) {
            return std::nullopt;
        }
        const auto    path = proc_root / std::to_string(pid) / "cmdline";
        std::ifstream input(path, std::ios::binary);
        if (!input.good()) {
            return std::nullopt;
        }
        std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (raw.empty()) {
            return std::nullopt;
        }
        std::string output;
        output.reserve(raw.size());
        bool pending_space = false;
        for (const char ch : raw) {
            if (ch == '\0') {
                pending_space = true;
                continue;
            }
            if (pending_space && !output.empty()) {
                output.push_back(' ');
            }
            pending_space = false;
            output.push_back(ch);
        }
        while (!output.empty() && output.back() == ' ') {
            output.pop_back();
        }
        if (output.empty()) {
            return std::nullopt;
        }
        return output;
    }

    void fill_client_commands(std::vector<ClientInfo>& clients, const std::filesystem::path& proc_root) {
        for (auto& client : clients) {
            if (client.command || !client.pid) {
                continue;
            }
            if (const auto command = read_process_cmdline(*client.pid, proc_root)) {
                client.command = *command;
            }
        }
    }

} // namespace hyprspaces
