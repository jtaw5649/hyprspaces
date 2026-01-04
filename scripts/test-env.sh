#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
STATE_DIR="${XDG_RUNTIME_DIR:-/tmp}/hyprspaces-dev"
PID_FILE="$STATE_DIR/nested.pid"
SIG_FILE="$STATE_DIR/nested.sig"

CONFIG="${HYPRSPACES_CONFIG:-$HOME/.config/hypr/hyprlandd.conf}"
NUM_DISPLAYS="${HYPRSPACES_DISPLAYS:-2}"

usage() {
    cat <<'EOF'
Usage: scripts/test-env.sh <command>

Launch a nested Hyprland session for plugin development with debug logging
and trace output enabled. Plugin is loaded ONLY in the nested session.

Commands:
  start [num]   Start nested Hyprland with N displays (default: 2)
  stop          Stop the nested session
  status        Show nested session status
  sig           Print the nested session's HYPRLAND_INSTANCE_SIGNATURE
  load          Build and load plugin into nested session
  unload        Unload plugin from nested session
  reload        Rebuild and reload plugin in nested session
  logs          Tail nested session logs (filtered for hyprspaces)
  test [delay]  Run test suite exercising all dispatchers (default delay: 0.5s)
  clean         Remove stale headless outputs from current session

Environment:
  HYPRSPACES_CONFIG     Config file (default: ~/.config/hypr/hyprlandd.conf)
  HYPRSPACES_DISPLAYS   Number of displays (default: 2)

Examples:
  ./scripts/test-env.sh start      # Start nested session with 2 displays
  ./scripts/test-env.sh load       # Build and load plugin
  ./scripts/test-env.sh logs       # Watch plugin logs
  ./scripts/test-env.sh reload     # Rebuild and hot-reload plugin
  ./scripts/test-env.sh stop       # Stop nested session

Note: The nested session uses ALT as modifier (not SUPER) to avoid conflicts.
EOF
}

check_deps() {
    local missing=()
    command -v Hyprland >/dev/null || missing+=("hyprland")

    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "error: missing dependencies: ${missing[*]}" >&2
        exit 1
    fi
}

get_nested_sig() {
    if [[ -f "$SIG_FILE" ]]; then
        cat "$SIG_FILE"
    else
        echo ""
    fi
}

nested_hyprctl() {
    local sig
    sig="$(get_nested_sig)"
    if [[ -z "$sig" ]]; then
        echo "error: nested session not running" >&2
        return 1
    fi
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl "$@"
}

cleanup_headless_outputs() {
    local sig="${1:-}"
    local cleanup_script="$SCRIPT_DIR/cleanup-headless.sh"

    if [[ ! -f "$cleanup_script" ]]; then
        echo "error: cleanup script not found: $cleanup_script" >&2
        return 1
    fi

    if [[ -n "$sig" ]]; then
        HYPRLAND_INSTANCE_SIGNATURE="$sig" bash "$cleanup_script"
    else
        bash "$cleanup_script"
    fi
}

start_env() {
    local num="${1:-$NUM_DISPLAYS}"

    if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "Nested session already running (PID $(cat "$PID_FILE"))"
        echo "Stop with: ./scripts/test-env.sh stop"
        exit 1
    fi

    check_deps
    mkdir -p "$STATE_DIR"

    if [[ ! -f "$CONFIG" ]]; then
        echo "error: config not found: $CONFIG" >&2
        echo "Copy fixtures/hyprlandd.conf to ~/.config/hypr/hyprlandd.conf" >&2
        exit 1
    fi

    echo "Starting nested Hyprland..."
    echo "  Config: $CONFIG"
    echo "  Displays: $num"
    echo "  Debug logging: enabled"
    echo "  Trace output: HYPRLAND_TRACE=1 AQ_TRACE=1"
    echo ""

    local log_file="$STATE_DIR/hyprland-nested.log"

    HYPRLAND_TRACE=1 \
    AQ_TRACE=1 \
    WLR_BACKENDS=wayland \
    WLR_WL_OUTPUTS="$num" \
    Hyprland -c "$CONFIG" >"$log_file" 2>&1 &

    local pid=$!
    echo "$pid" > "$PID_FILE"

    echo "Waiting for nested session to initialize..."
    sleep 2

    local sig=""
    for _ in {1..10}; do
        sig=$(ls -t "$XDG_RUNTIME_DIR/hypr/" 2>/dev/null | head -1)
        if [[ -n "$sig" && "$sig" != "$HYPRLAND_INSTANCE_SIGNATURE" ]]; then
            local socket="$XDG_RUNTIME_DIR/hypr/$sig/.socket.sock"
            if [[ -S "$socket" ]]; then
                break
            fi
        fi
        sig=""
        sleep 0.5
    done

    if [[ -z "$sig" ]]; then
        echo "error: failed to detect nested session signature" >&2
        kill "$pid" 2>/dev/null || true
        rm -f "$PID_FILE"
        exit 1
    fi

    echo "$sig" > "$SIG_FILE"

    echo ""
    echo "Nested session started!"
    echo "  PID: $pid"
    echo "  Signature: $sig"
    echo "  Log: $log_file"
    echo ""
    echo "Commands:"
    echo "  ./scripts/test-env.sh load     # Build and load plugin"
    echo "  ./scripts/test-env.sh logs     # Watch plugin logs"
    echo "  ./scripts/test-env.sh reload   # Rebuild and reload"
    echo "  ./scripts/test-env.sh stop     # Stop session"
    echo ""
    echo "Manual hyprctl in nested session:"
    echo "  HYPRLAND_INSTANCE_SIGNATURE=\"$sig\" hyprctl ..."
}

stop_env() {
    if [[ ! -f "$PID_FILE" ]]; then
        echo "No nested session running."
        return 0
    fi

    local pid sig
    pid=$(cat "$PID_FILE")
    sig=$(get_nested_sig)

    echo "Stopping nested session (PID $pid)..."

    if [[ -n "$sig" ]] && kill -0 "$pid" 2>/dev/null; then
        cleanup_headless_outputs "$sig"
    fi

    if kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
        sleep 1
        kill -9 "$pid" 2>/dev/null || true
    fi

    rm -f "$PID_FILE" "$SIG_FILE"
    echo "Nested session stopped."
}

show_status() {
    echo "=== Hyprspaces Development Environment ==="
    echo ""

    if [[ ! -f "$PID_FILE" ]]; then
        echo "Status: Not running"
        echo ""
        echo "Start with: ./scripts/test-env.sh start"
        return 0
    fi

    local pid sig
    pid=$(cat "$PID_FILE")
    sig=$(get_nested_sig)

    if ! kill -0 "$pid" 2>/dev/null; then
        echo "Status: Stale (process $pid not running)"
        rm -f "$PID_FILE" "$SIG_FILE"
        return 1
    fi

    echo "Status: Running"
    echo "  PID: $pid"
    echo "  Signature: $sig"
    echo ""

    echo "Monitors:"
    nested_hyprctl monitors -j 2>/dev/null | jq -r '.[] | "  \(.name) (\(.width)x\(.height))"' || echo "  (unable to query)"
    echo ""

    echo "Plugin status:"
    local plugins
    plugins=$(nested_hyprctl plugins list 2>/dev/null || echo "")
    if [[ "$plugins" == "no plugins loaded" || -z "$plugins" ]]; then
        echo "  No plugins loaded"
        echo "  Load with: ./scripts/test-env.sh load"
    else
        echo "$plugins" | sed 's/^/  /'
    fi
}

print_sig() {
    local sig
    sig=$(get_nested_sig)
    if [[ -z "$sig" ]]; then
        echo "error: nested session not running" >&2
        exit 1
    fi
    echo "$sig"
}

setup_dual_monitors() {
    local sig="$1"
    local monitors
    monitors=$(HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl monitors -j 2>/dev/null)
    local count
    count=$(echo "$monitors" | jq 'length')

    if [[ "$count" -lt 2 ]]; then
        echo "Creating second monitor (HEADLESS)..."
        HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl output create headless >/dev/null 2>&1
        sleep 0.3
    fi

    echo "Configuring workspace-monitor bindings..."
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl --batch "\
        keyword workspace 1,monitor:WAYLAND-1,default:true ; \
        keyword workspace 2,monitor:HEADLESS-2,default:true ; \
        keyword workspace 3,monitor:WAYLAND-1 ; \
        keyword workspace 4,monitor:HEADLESS-2 ; \
        keyword workspace 5,monitor:WAYLAND-1 ; \
        keyword workspace 6,monitor:HEADLESS-2 ; \
        keyword workspace 7,monitor:WAYLAND-1 ; \
        keyword workspace 8,monitor:HEADLESS-2 ; \
        keyword workspace 9,monitor:WAYLAND-1 ; \
        keyword workspace 10,monitor:HEADLESS-2" >/dev/null 2>&1 || true
}

load_plugin() {
    local sig
    sig=$(get_nested_sig)
    if [[ -z "$sig" ]]; then
        echo "error: nested session not running" >&2
        echo "Start with: ./scripts/test-env.sh start" >&2
        exit 1
    fi

    local build_dir="${HYPRSPACES_BUILD_DIR:-build}"

    echo "Building plugin..."
    cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/$build_dir" >/dev/null
    cmake --build "$PROJECT_DIR/$build_dir"

    local plugin_path
    plugin_path="$(realpath "$PROJECT_DIR/$build_dir/hyprspaces.so")"

    setup_dual_monitors "$sig"

    echo "Loading plugin into nested session..."
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl plugin unload "$plugin_path" >/dev/null 2>&1 || true
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl plugin load "$plugin_path"

    local binds_file="$HOME/.config/hypr/hyprspaces-binds.conf"
    if [[ -f "$binds_file" ]]; then
        echo "Loading keybindings..."
        HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl keyword source "$binds_file" >/dev/null 2>&1 || true
    fi

    echo ""
    echo "Plugin loaded with dual-monitor setup:"
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl monitors -j 2>/dev/null | jq -r '.[] | "  \(.name) at \(.x),\(.y)"' || true
    echo ""
    echo "Keybindings active (ALT+1-5, ALT+C, etc.)"
    echo "Test with: ALT+1 in nested window, or:"
    echo "  HYPRLAND_INSTANCE_SIGNATURE=\"$sig\" hyprctl dispatch hyprspaces:paired_switch 1"
}

unload_plugin() {
    local sig
    sig=$(get_nested_sig)
    if [[ -z "$sig" ]]; then
        echo "error: nested session not running" >&2
        exit 1
    fi

    local build_dir="${HYPRSPACES_BUILD_DIR:-build}"
    local plugin_path
    plugin_path="$(realpath "$PROJECT_DIR/$build_dir/hyprspaces.so" 2>/dev/null || echo "")"

    if [[ -z "$plugin_path" ]]; then
        echo "error: plugin not built" >&2
        exit 1
    fi

    echo "Unloading plugin from nested session..."
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl plugin unload "$plugin_path"
}

reload_plugin() {
    load_plugin
}

tail_logs() {
    local sig
    sig=$(get_nested_sig)
    if [[ -z "$sig" ]]; then
        echo "error: nested session not running" >&2
        exit 1
    fi

    local log_path="$XDG_RUNTIME_DIR/hypr/$sig/hyprland.log"
    local pattern='\[hyprspaces\]|hyprspaces:|plugin|Plugin|(ERROR|WARN|CRIT)'

    if [[ -f "$log_path" ]]; then
        echo "Tailing: $log_path"
        echo "Filter: $pattern"
        echo "---"
        tail -f "$log_path" | grep -E --line-buffered "$pattern"
    else
        echo "error: log file not found: $log_path" >&2
        exit 1
    fi
}

run_tests() {
    local sig
    sig=$(get_nested_sig)
    if [[ -z "$sig" ]]; then
        echo "error: nested session not running" >&2
        exit 1
    fi

    RUN_TESTS_SIG="$sig"

    local delay="${1:-0.5}"
    local hypr_log="$XDG_RUNTIME_DIR/hypr/$sig/hyprland.log"
    local pattern='\[hyprspaces\]|hyprspaces:|plugin|Plugin|(ERROR|WARN|CRIT)'
    local test_log="$STATE_DIR/test-$(date +%Y%m%d-%H%M%S).log"
    local tail_pid=""

    cleanup() {
        [[ -n "${tail_pid:-}" ]] && kill "$tail_pid" 2>/dev/null || true
        cleanup_headless_outputs "${RUN_TESTS_SIG:-}"
    }
    trap cleanup EXIT INT TERM

    if [[ -f "$hypr_log" ]]; then
        tail -f "$hypr_log" 2>/dev/null | grep -E --line-buffered "$pattern" >> "$test_log" &
        tail_pid=$!
    fi

    dispatch() {
        echo "→ $1 $2"
        echo "[$(date +%H:%M:%S)] dispatch $1 $2" >> "$test_log"
        HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl dispatch "$1" "$2" 2>&1 | grep -v "^ok$" || true
        sleep "$delay"
    }

    echo "=== Hyprspaces Plugin Test Suite ==="
    echo "Log file: $test_log"
    echo ""

    echo "[1/6] Testing paired_switch (workspaces 1-5)..."
    for i in 1 2 3 4 5; do
        dispatch "hyprspaces:paired_switch" "$i"
    done

    echo "[2/6] Testing paired_cycle..."
    dispatch "hyprspaces:paired_cycle" "next"
    dispatch "hyprspaces:paired_cycle" "next"
    dispatch "hyprspaces:paired_cycle" "prev"
    dispatch "hyprspaces:paired_cycle" "prev"

    echo "[3/6] Testing session_save..."
    dispatch "hyprspaces:session_save" ""

    echo "[4/6] Testing paired_switch again (verify state)..."
    dispatch "hyprspaces:paired_switch" "2"
    dispatch "hyprspaces:paired_switch" "1"

    echo "[5/6] Testing session_restore..."
    dispatch "hyprspaces:session_restore" ""

    echo "[6/6] Testing grab_rogue..."
    dispatch "hyprspaces:grab_rogue" ""

    sleep 0.3
    cleanup
    echo ""
    echo "=== Test Suite Complete ==="
    echo ""
    echo "Final workspace state:"
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl workspaces -j 2>/dev/null | \
        jq -r '.[] | "  WS \(.id) on \(.monitor)"' || true
    echo ""
    echo "Logs saved to: $test_log"
    local errors warnings
    errors=$(grep -cE '(ERROR|CRIT)' "$test_log" 2>/dev/null) || errors=0
    warnings=$(grep -cE 'WARN' "$test_log" 2>/dev/null) || warnings=0
    echo "Summary: $errors errors, $warnings warnings"
}

clean_headless() {
    cleanup_headless_outputs ""
}

command="${1:-}"

case "$command" in
    start)
        start_env "${2:-}"
        ;;
    stop)
        stop_env
        ;;
    status)
        show_status
        ;;
    sig)
        print_sig
        ;;
    load)
        load_plugin
        ;;
    unload)
        unload_plugin
        ;;
    reload)
        reload_plugin
        ;;
    logs)
        tail_logs
        ;;
    test)
        run_tests "${2:-0.5}"
        ;;
    clean)
        clean_headless
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        usage
        exit 1
        ;;
esac
