#pragma once

#include <unistd.h>

namespace hyprspaces {

    class FileDescriptor {
      public:
        FileDescriptor() = default;
        explicit FileDescriptor(int fd) : fd_(fd) {}
        ~FileDescriptor() {
            if (fd_ >= 0) {
                ::close(fd_);
            }
        }

        FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
            other.fd_ = -1;
        }

        int get() const {
            return fd_;
        }
        explicit operator bool() const {
            return fd_ >= 0;
        }

        void reset(int fd = -1) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = fd;
        }

      private:
        int fd_ = -1;
    };

}
