#include <atomic>
#include <cerrno>
#include <filesystem>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "hyprspaces/waybar_socket.hpp"

namespace {

    std::filesystem::path temp_socket_path() {
        static std::atomic<int> counter{0};
        const auto              suffix = std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1));
        return std::filesystem::temp_directory_path() / ("hyprspaces-waybar-" + suffix + ".sock");
    }

    int connect_client(const std::filesystem::path& path) {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
        sockaddr_un addr{};
        addr.sun_family               = AF_UNIX;
        const std::string socket_path = path.string();
        std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    std::string read_exact(int fd, size_t size) {
        std::string data;
        data.resize(size);
        size_t offset = 0;
        while (offset < size) {
            pollfd pfd{};
            pfd.fd          = fd;
            pfd.events      = POLLIN;
            const int ready = ::poll(&pfd, 1, 1000);
            if (ready <= 0) {
                break;
            }
            const ssize_t result = ::recv(fd, data.data() + offset, size - offset, 0);
            if (result <= 0) {
                break;
            }
            offset += static_cast<size_t>(result);
        }
        data.resize(offset);
        return data;
    }

    struct SendShim {
        static std::atomic<int> calls;

        static ssize_t          send(int fd, const void* buffer, size_t size, int flags) {
            if (calls.fetch_add(1) == 0) {
                errno = EAGAIN;
                return -1;
            }
            return ::send(fd, buffer, size, flags);
        }
    };

    std::atomic<int> SendShim::calls{0};

} // namespace

TEST(WaybarSocketServer, AcceptsAndBroadcasts) {
    const auto                     path = temp_socket_path();
    hyprspaces::WaybarSocketServer server(path);
    ASSERT_FALSE(server.start().has_value());

    const int client = connect_client(path);
    ASSERT_GE(client, 0);

    server.accept_ready();
    server.broadcast("hello");

    EXPECT_EQ(read_exact(client, 5), "hello");

    ::close(client);
    server.stop();
}

TEST(WaybarSocketServer, SendsLastMessageOnAccept) {
    const auto                     path = temp_socket_path();
    hyprspaces::WaybarSocketServer server(path);
    ASSERT_FALSE(server.start().has_value());

    server.broadcast("first");

    const int client = connect_client(path);
    ASSERT_GE(client, 0);
    server.accept_ready();

    EXPECT_EQ(read_exact(client, 5), "first");

    ::close(client);
    server.stop();
}

TEST(WaybarSocketServer, DropsDisconnectedClient) {
    const auto                     path = temp_socket_path();
    hyprspaces::WaybarSocketServer server(path);
    ASSERT_FALSE(server.start().has_value());

    const int stale_client = connect_client(path);
    ASSERT_GE(stale_client, 0);
    server.accept_ready();
    ::close(stale_client);

    server.broadcast("hello");

    const int client = connect_client(path);
    ASSERT_GE(client, 0);
    server.accept_ready();
    server.broadcast("ok");

    EXPECT_EQ(read_exact(client, 5), "hello");
    EXPECT_EQ(read_exact(client, 2), "ok");

    ::close(client);
    server.stop();
}

TEST(WaybarSocketServer, RetainsClientOnWouldBlock) {
    const auto                     path = temp_socket_path();
    hyprspaces::WaybarSocketServer server(path);
    ASSERT_FALSE(server.start().has_value());

    const int client = connect_client(path);
    ASSERT_GE(client, 0);
    server.accept_ready();

    SendShim::calls.store(0);
    hyprspaces::set_waybar_socket_send_fn_for_tests(&SendShim::send);

    server.broadcast("hello");
    server.broadcast("world");

    EXPECT_EQ(read_exact(client, 5), "hello");
    EXPECT_EQ(read_exact(client, 5), "world");

    hyprspaces::reset_waybar_socket_send_fn_for_tests();
    ::close(client);
    server.stop();
}
