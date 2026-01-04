#!/usr/bin/env bash
set -euo pipefail

if ! command -v hyprctl >/dev/null 2>&1; then
    echo "error: hyprctl not found in PATH" >&2
    exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "error: jq not found in PATH" >&2
    exit 1
fi

echo "Checking for stale headless outputs in current session..."
removed=0

while IFS= read -r output; do
    [[ -z "$output" ]] && continue
    echo "  Removing $output..."
    hyprctl output remove "$output" 2>/dev/null || true
    removed=$((removed + 1))
done < <(hyprctl monitors -j | jq -r '.[] | select(.name | startswith("HEADLESS")) | .name')

if [[ $removed -eq 0 ]]; then
    echo "No stale headless outputs found."
else
    echo "Removed $removed headless output(s)."
fi
