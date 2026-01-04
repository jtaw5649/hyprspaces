#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Install clang-format and retry." >&2
  exit 1
fi

if command -v rg >/dev/null 2>&1; then
  mapfile -t files < <(rg --files -g '*.{c,cc,cpp,cxx,h,hh,hpp,ipp}')
else
  mapfile -t files < <(find . -type f \( \
    -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o \
    -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.ipp' \
    \) -not -path './.git/*' -not -path './build/*' -not -path './.cache/*')
fi

if [[ ${#files[@]} -eq 0 ]]; then
  echo "No C/C++ files found to format."
  exit 0
fi

clang-format -i "${files[@]}"
