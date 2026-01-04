#!/usr/bin/env python3

import os
import socket
import sys


def socket_path():
    xdg_config = os.environ.get("XDG_CONFIG_HOME")
    if xdg_config:
        return os.path.join(xdg_config, "hyprspaces", "waybar", "waybar.sock")
    home = os.environ.get("HOME")
    if not home:
        return None
    return os.path.join(home, ".config", "hyprspaces", "waybar", "waybar.sock")


path = socket_path()
if not path:
    sys.exit("missing XDG_CONFIG_HOME/HOME")

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(path)

while True:
    data = sock.recv(4096)
    if not data:
        break
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()
