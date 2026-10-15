#!/usr/bin/env bash
# Attach to an already running, uninstrumented game. Never changes host policy.
set -euo pipefail
mode=${1:-help}
seconds=${2:-30}
perf_bin=${PERF_BIN:-perf}
out=${OUT_DIR:-"$PWD/perf-results"}
case "$mode" in
    stat|record|c2c|mem|sched|stacks) ;;
    *)
        cat <<'EOF'
Usage: PID=1234 tools/profile_gameplay.sh stat|record|c2c|mem|sched|stacks [seconds]
Run from build/bin, or set OUT_DIR. PERF_BIN selects an installed perf binary.
Use a RelWithDebInfo build with GPP_PROFILE_GAMEPLAY=OFF, GPP_NATIVE_CPU=OFF.
Use identical assets, bots, chart, FPS limit, BPM option and affinity for A/B.
Repeat at least five times. Keep the exact executable and build ID with results.
Optional: PERF_EVENT=cycles:u, PERF_CALLGRAPH=dwarf,8192, PERF_FREQ=99.
stat: set EVENTS to a CPU-supported event list; generic cache-misses != L1/LLC.
For per-thread counters use TID=1235 (separate runs). perf list shows PMU events.
c2c/mem support depends on CPU/kernel; no samples does not prove no sharing.
sched and stacks are correctness/scheduling diagnostics, not timing baselines.
EOF
        exit 0 ;;
esac
[[ "$seconds" =~ ^[1-9][0-9]*$ ]] || { echo "Invalid duration" >&2; exit 2; }
pid=${PID:-$(pgrep -n -x GuitarPP || true)}
[[ -n "$pid" && -d /proc/$pid ]] || { echo "Set PID to the running game" >&2; exit 2; }
command -v "$perf_bin" >/dev/null
mkdir -p "$out"
run="$out/$(date +%Y%m%d-%H%M%S)-$pid-$mode"
mkdir "$run"
readlink "/proc/$pid/exe" > "$run/executable.txt"
readelf -n "/proc/$pid/exe" > "$run/build-id.txt"
ps -L -p "$pid" -o pid,tid,psr,comm > "$run/threads.txt"
lscpu > "$run/cpu.txt"
"$perf_bin" list > "$run/events.txt"
target=(-p "$pid")
if [[ -n "${TID:-}" ]]; then
    target=(-t "$TID")
fi
case "$mode" in
    stat)
        "$perf_bin" stat -x, -o "$run/stat.csv" -e "${EVENTS:-cycles:u,instructions:u,cache-references:u,cache-misses:u,L1-dcache-loads:u,L1-dcache-load-misses:u,LLC-loads:u,LLC-load-misses:u,branches:u,branch-misses:u,context-switches,cpu-migrations,task-clock}" "${target[@]}" -- sleep "$seconds" ;;
    record)
        "$perf_bin" record -o "$run/perf.data" -e "${PERF_EVENT:-cache-misses:u}" -F "${PERF_FREQ:-99}" --call-graph "${PERF_CALLGRAPH:-dwarf,8192}" "${target[@]}" -- sleep "$seconds"
        "$perf_bin" report --stdio --no-children --call-graph none -i "$run/perf.data" > "$run/report.txt" ;;
    c2c|mem)
        "$perf_bin" "$mode" record -o "$run/perf.data" "${target[@]}" -- sleep "$seconds"
        "$perf_bin" "$mode" report --stdio -i "$run/perf.data" > "$run/report.txt" ;;
    sched)
        "$perf_bin" sched record -o "$run/perf.data" -p "$pid" -- sleep "$seconds"
        "$perf_bin" sched timehist -i "$run/perf.data" > "$run/report.txt" ;;
    stacks)
        gdb -nx -batch -p "$pid" -ex 'set pagination off' -ex 'thread apply all bt' -ex detach > "$run/stacks.txt" 2>&1 ;;
esac
echo "Results: $run"
