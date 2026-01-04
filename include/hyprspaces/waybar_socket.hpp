#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <sys/types.h>

#include "hyprspaces/file_descriptor.hpp"

namespace hyprspaces {

    class WaybarSocketServer {
      public:
        explicit WaybarSocketServer(std::filesystem::path socket_path);
        ~WaybarSocketServer();

        std::optional<std::string> start();
        void                       stop();
        void                       broadcast(std::string_view message);
        void                       accept_ready();

        bool                       running() const {
            return running_;
        }
        int server_fd() const {
            return server_fd_.get();
        }
        const std::filesystem::path& socket_path() const {
            return socket_path_;
        }

      private:
        struct ClientState {
            int         fd = -1;
            std::string pending;
        };

        enum class SendResult {
            kOk,
            kWouldBlock,
            kError,
        };

        SendResult               send_all(ClientState& client, std::string_view message);
        bool                     accept_one();
        void                     close_client(int fd);
        static bool              set_nonblocking(int fd);
        static bool              set_cloexec(int fd);

        std::filesystem::path    socket_path_;
        FileDescriptor           server_fd_;
        std::vector<ClientState> clients_;
        std::string              last_message_;
        bool                     running_ = false;
    };

#ifdef HYPRSPACES_TESTING
    using WaybarSocketSendFn = ssize_t (*)(int, const void*, size_t, int);

    void set_waybar_socket_send_fn_for_tests(WaybarSocketSendFn fn);
    void reset_waybar_socket_send_fn_for_tests();
#endif

} // namespace hyprspaces
