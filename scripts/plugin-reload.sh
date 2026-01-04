#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
STATE_DIR="${XDG_RUNTIME_DIR:-/tmp}/hyprspaces-dev"
SIG_FILE="$STATE_DIR/nested.sig"

build_dir="${HYPRSPACES_BUILD_DIR:-build}"

get_target_sig() {
    if [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]]; then
        echo "$HYPRLAND_INSTANCE_SIGNATURE"
    elif [[ -f "$SIG_FILE" ]]; then
        cat "$SIG_FILE"
    else
        echo ""
    fi
}

sig=$(get_target_sig)
if [[ -z "$sig" ]]; then
    echo "error: no Hyprland session found" >&2
    echo "Either:" >&2
    echo "  - Run from within a Hyprland session" >&2
    echo "  - Start nested session: ./scripts/test-env.sh start" >&2
    exit 1
fi

echo "Building plugin..."
cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/$build_dir" >/dev/null
cmake --build "$PROJECT_DIR/$build_dir"

plugin_path="$(realpath "$PROJECT_DIR/$build_dir/hyprspaces.so")"

echo "Reloading plugin in session: ${sig:0:12}..."
HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl plugin unload "$plugin_path" >/dev/null 2>&1 || true
HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl plugin load "$plugin_path"
