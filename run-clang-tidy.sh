#!/usr/bin/env bash

set -euo pipefail

# Usage: ./run-clang-tidy.sh [build-directory] [jobs] [git-since]
# Analyze current sources touched by commits reachable from HEAD in the period.
# Headers are checked through the selected translation units, not standalone.
# Enable optional CMake targets to include their sources in compile_commands.json.
project_root="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly project_root
readonly build_directory="${1:-${project_root}/build}"
readonly jobs="${2:-4}"
readonly since="${3:-7 days ago}"

if [[ $# -gt 3 || ! "${jobs}" =~ ^[1-9][0-9]*$ ]]; then
    echo "Usage: $0 [build-directory] [positive-jobs] [git-since]" >&2
    exit 2
fi

if [[ ! -f "${build_directory}/compile_commands.json" ]]; then
    echo "Missing ${build_directory}/compile_commands.json; configure CMake first." >&2
    exit 2
fi

# Use NUL-delimited Git output and escaped, anchored paths for the regex.
# Report sources absent from the database instead of silently omitting them.
source_filter="$(python3 - "${project_root}" "${build_directory}" "${since}" <<'PY'
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(sys.argv[1])
database = json.loads((Path(sys.argv[2]) / "compile_commands.json").read_text())
compiled = {
    (Path(entry["directory"]) / entry["file"]).resolve()
    for entry in database
}
changed = subprocess.check_output([
    "git", "-C", str(root), "log", "--since=" + sys.argv[3],
    "--format=", "--name-only", "-z", "--diff-filter=ACMRT", "--",
    "src", "tests", "tools",
])
sources = []
for name in sorted(set(changed.decode().split("\0"))):
    if not name or name.startswith(("src/Lua/", "src/utf8/")):
        continue
    path = root / name
    if path.suffix not in {".c", ".cc", ".cpp", ".cxx"} or not path.is_file():
        continue
    if path.resolve() not in compiled:
        print("Not in compilation database: " + name, file=sys.stderr)
        continue
    sources.append(str(path))
print(f"Analyzing {len(sources)} translation units (since {sys.argv[3]}).", file=sys.stderr)
print("^(" + "|".join(re.escape(path) for path in sources) + ")$" if sources else "")
PY
)"
readonly source_filter

if [[ -z "${source_filter}" ]]; then
    exit 0
fi

header_root="$(python3 -c 'import re, sys; print(re.escape(sys.argv[1]))' "${project_root}")"
readonly header_root

exec run-clang-tidy \
    -p "${build_directory}" \
    -j "${jobs}" \
    -source-filter "${source_filter}" \
    -header-filter "^${header_root}/(src|tests|tools)/" \
    -exclude-header-filter "^${header_root}/(src/(Lua|utf8)|vendor|glfw|glm|cereal)/" \
    -use-color=false
