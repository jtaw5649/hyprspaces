FROM archlinux:latest

RUN pacman -Syu --noconfirm --needed \
    base-devel \
    git \
    cmake \
    ninja \
    gcc \
    clang \
    lld \
    gdb \
    pkgconf \
    pixman \
    python \
    wayland \
    wayland-protocols \
    hyprland \
  && pacman -Scc --noconfirm
