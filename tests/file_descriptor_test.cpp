#include "hyprspaces/file_descriptor.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <utility>

#include <gtest/gtest.h>

namespace {

    TEST(FileDescriptor, DefaultConstructsInvalid) {
        hyprspaces::FileDescriptor fd;
        EXPECT_EQ(fd.get(), -1);
        EXPECT_FALSE(static_cast<bool>(fd));
    }

    TEST(FileDescriptor, ConstructsFromValidFd) {
        int pipe_fds[2];
        ASSERT_EQ(::pipe(pipe_fds), 0);
        hyprspaces::FileDescriptor fd(pipe_fds[0]);
        EXPECT_EQ(fd.get(), pipe_fds[0]);
        EXPECT_TRUE(static_cast<bool>(fd));
        ::close(pipe_fds[1]);
    }

    TEST(FileDescriptor, ClosesOnDestruction) {
        int pipe_fds[2];
        ASSERT_EQ(::pipe(pipe_fds), 0);
        int read_fd = pipe_fds[0];
        { hyprspaces::FileDescriptor fd(read_fd); }
        EXPECT_EQ(::fcntl(read_fd, F_GETFD), -1);
        ::close(pipe_fds[1]);
    }

    TEST(FileDescriptor, MoveConstructsTransfersOwnership) {
        int pipe_fds[2];
        ASSERT_EQ(::pipe(pipe_fds), 0);
        int                        read_fd = pipe_fds[0];
        hyprspaces::FileDescriptor fd1(read_fd);
        hyprspaces::FileDescriptor fd2(std::move(fd1));
        EXPECT_EQ(fd1.get(), -1);
        EXPECT_EQ(fd2.get(), read_fd);
        ::close(pipe_fds[1]);
    }

    TEST(FileDescriptor, ResetClosesOldAndTakesNew) {
        int pipe1[2];
        int pipe2[2];
        ASSERT_EQ(::pipe(pipe1), 0);
        ASSERT_EQ(::pipe(pipe2), 0);
        int                        old_fd = pipe1[0];
        int                        new_fd = pipe2[0];
        hyprspaces::FileDescriptor fd(old_fd);
        fd.reset(new_fd);
        EXPECT_EQ(fd.get(), new_fd);
        EXPECT_EQ(::fcntl(old_fd, F_GETFD), -1);
        ::close(pipe1[1]);
        ::close(pipe2[1]);
    }

}
