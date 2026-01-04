# hyprspaces

hyprspaces is an in-process Hyprland plugin that replaces the Rust CLI and makes multi-monitor workspace
management and session state feel native to the compositor.

For the current stable Rust CLI, see https://github.com/jtaw5649/hyprspaces-rs.

## Mission

- Provide predictable, group-based workspace switching across N monitors with consistent mappings.
- Deliver session save/restore using Wayland xx-session-management-v1 (opt-in, no legacy formats).
- Keep setup fileless: apply runtime workspace rules via ConfigManager and keyword-driven profiles.
- Keep opt-in features purely config-driven (Hyprland config + plugin values), no CLI configuration.
- Stream continuous Waybar state through the socket helper at `~/.config/hyprspaces/waybar/`.
- Prioritize stability: guard plugin entrypoints, fail closed, and emit comprehensive debug/error logs.

## Scope

- x86_64 only.
- C++26, experimental features allowed.
- No backward-compatibility shims or legacy persistence formats.

## Architecture

- Prefer function hooks on x86_64 to maximize native integration; fall back to event hooks only when required.
- Waybar updates are event-driven with cached theme reads.

## Development

### Devcontainer (Arch)

Use the VS Code Dev Containers config. It includes Hyprland headers, pixman,
python, and gdb so the plugin target can build.

Build and run tests:

```sh
./scripts/test.sh
```

### Nix dev shell

```sh
nix develop
./scripts/test.sh
```

### Multi-monitor test environment

Create headless test monitors streamed via wayvnc for multi-display testing:

```sh
sudo pacman -S wayvnc
./scripts/test-env.sh start      # Create 2 test displays
./scripts/test-env.sh start 3    # Or specify number of displays
./scripts/test-env.sh status     # Show connection info
./scripts/test-env.sh stop       # Clean up
```

Connect with any VNC viewer (e.g., `gnome-connections vnc://127.0.0.1:5901`).

Environment variables:
- `HYPRSPACES_TEST_DISPLAYS` - Number of displays (default: 2)
- `HYPRSPACES_TEST_PORT` - Starting VNC port (default: 5901)
- `HYPRSPACES_TEST_WORKSPACE` - Starting workspace number (default: 11)
- `HYPRSPACES_TEST_RESOLUTION` - Monitor resolution (default: 1920x1080@60)

### Manual plugin testing

Build the plugin:

```sh
cmake -S . -B build
cmake --build build
```

Load/unload in a running Hyprland session:

```sh
hyprctl plugin load "$(realpath build/hyprspaces.so)"
hyprctl plugin unload "$(realpath build/hyprspaces.so)"
```
