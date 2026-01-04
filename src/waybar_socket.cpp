#include "hyprspaces/waybar_socket.hpp"

#include <cerrno>
#include <cstring>
#include <system_error>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace hyprspaces {

    namespace {

        using SendFn = ssize_t (*)(int, const void*, size_t, int);

        SendFn g_send_fn = ::send;

    } // namespace

#ifdef HYPRSPACES_TESTING
    void set_waybar_socket_send_fn_for_tests(WaybarSocketSendFn fn) {
        g_send_fn = fn ? fn : ::send;
    }

    void reset_waybar_socket_send_fn_for_tests() {
        g_send_fn = ::send;
    }
#endif

    WaybarSocketServer::WaybarSocketServer(std::filesystem::path socket_path) : socket_path_(std::move(socket_path)) {}

    WaybarSocketServer::~WaybarSocketServer() {
        stop();
    }

    std::optional<std::string> WaybarSocketServer::start() {
        if (running_) {
            return std::nullopt;
        }

        try {
            if (const auto parent = socket_path_.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent);
            }
            if (std::filesystem::exists(socket_path_)) {
                std::filesystem::remove(socket_path_);
            }
        } catch (const std::exception& ex) { return ex.what(); }

        server_fd_.reset(::socket(AF_UNIX, SOCK_STREAM, 0));
        if (!server_fd_) {
            return "unable to create waybar socket";
        }
        if (!set_cloexec(server_fd_.get()) || !set_nonblocking(server_fd_.get())) {
            server_fd_.reset();
            return "unable to configure waybar socket";
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const auto path = socket_path_.string();
        if (path.size() >= sizeof(addr.sun_path)) {
            server_fd_.reset();
            return "waybar socket path too long";
        }
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (::bind(server_fd_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            server_fd_.reset();
            return "unable to bind waybar socket";
        }
        if (::listen(server_fd_.get(), 4) < 0) {
            server_fd_.reset();
            return "unable to listen on waybar socket";
        }

        running_ = true;
        return std::nullopt;
    }

    void WaybarSocketServer::stop() {
        if (!running_) {
            return;
        }
        running_ = false;

        for (const auto& client : clients_) {
            ::close(client.fd);
        }
        clients_.clear();

        server_fd_.reset();

        std::error_code ec;
        std::filesystem::remove(socket_path_, ec);
    }

    void WaybarSocketServer::accept_ready() {
        if (!running_ || !server_fd_) {
            return;
        }
        while (accept_one()) {}
    }

    void WaybarSocketServer::broadcast(std::string_view message) {
        if (!running_) {
            return;
        }
        accept_ready();
        last_message_.assign(message.begin(), message.end());
        for (auto it = clients_.begin(); it != clients_.end();) {
            const auto result = send_all(*it, message);
            if (result == SendResult::kError) {
                close_client(it->fd);
                it = clients_.erase(it);
                continue;
            }
            ++it;
        }
    }

    WaybarSocketServer::SendResult WaybarSocketServer::send_all(ClientState& client, std::string_view message) {
        if (!message.empty()) {
            client.pending.append(message.begin(), message.end());
        }
        size_t sent = 0;
        while (sent < client.pending.size()) {
            const ssize_t result = g_send_fn(client.fd, client.pending.data() + sent, client.pending.size() - sent, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (result > 0) {
                sent += static_cast<size_t>(result);
                continue;
            }
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (sent > 0) {
                    client.pending.erase(0, sent);
                }
                return SendResult::kWouldBlock;
            }
            return SendResult::kError;
        }
        client.pending.clear();
        return SendResult::kOk;
    }

    bool WaybarSocketServer::accept_one() {
        const int client_fd = ::accept(server_fd_.get(), nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) {
                return true;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return false;
            }
            return false;
        }
        if (!set_cloexec(client_fd) || !set_nonblocking(client_fd)) {
            ::close(client_fd);
            return true;
        }
        ClientState client{.fd = client_fd, .pending = {}};
        if (!last_message_.empty()) {
            const auto result = send_all(client, last_message_);
            if (result == SendResult::kError) {
                ::close(client_fd);
                return true;
            }
        }
        clients_.push_back(std::move(client));
        return true;
    }

    void WaybarSocketServer::close_client(int fd) {
        if (fd >= 0) {
            ::close(fd);
        }
    }

    bool WaybarSocketServer::set_nonblocking(int fd) {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            return false;
        }
        return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
    }

    bool WaybarSocketServer::set_cloexec(int fd) {
        const int flags = ::fcntl(fd, F_GETFD, 0);
        if (flags < 0) {
            return false;
        }
        return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
    }

} // namespace hyprspaces
