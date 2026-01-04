#!/usr/bin/env bash
set -euo pipefail

STATE_DIR="${XDG_RUNTIME_DIR:-/tmp}/hyprspaces-dev"
SIG_FILE="$STATE_DIR/nested.sig"

mode="${1:-}"
pattern="${HYPRSPACES_LOG_GREP:-\\[hyprspaces\\]|hyprspaces:|plugin|Plugin|(ERROR|WARN|CRIT)}"

usage() {
    cat <<'EOF'
Usage: scripts/plugin-logs.sh [mode]

Modes:
  nested    Tail nested session log (default if nested session running)
  current   Tail current session log
  rolling   Follow hyprctl rollinglog

Environment:
  HYPRSPACES_LOG_GREP   ERE filter pattern
EOF
}

get_nested_sig() {
    if [[ -f "$SIG_FILE" ]]; then
        cat "$SIG_FILE"
    else
        echo ""
    fi
}

get_target_sig() {
    local nested_sig
    nested_sig=$(get_nested_sig)

    case "$mode" in
        nested)
            echo "$nested_sig"
            ;;
        current)
            echo "${HYPRLAND_INSTANCE_SIGNATURE:-}"
            ;;
        "")
            if [[ -n "$nested_sig" ]]; then
                echo "$nested_sig"
            else
                echo "${HYPRLAND_INSTANCE_SIGNATURE:-}"
            fi
            ;;
        *)
            echo ""
            ;;
    esac
}

case "$mode" in
    -h|--help)
        usage
        exit 0
        ;;
    rolling)
        exec hyprctl rollinglog -f | grep -E --line-buffered "$pattern"
        ;;
esac

sig=$(get_target_sig)

if [[ -z "$sig" ]]; then
    echo "error: no Hyprland session found" >&2
    echo "Start nested session: ./scripts/test-env.sh start" >&2
    exit 1
fi

log_path="$XDG_RUNTIME_DIR/hypr/$sig/hyprland.log"

if [[ ! -f "$log_path" ]]; then
    echo "error: log not found: $log_path" >&2
    exit 1
fi

echo "Tailing: $log_path"
echo "Filter: $pattern"
echo "---"
exec tail -f "$log_path" | grep -E --line-buffered "$pattern"
