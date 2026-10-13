#!/bin/sh
# Start ce_cores and the Go webserver together.
# Paths default to the values in .env (copied to BIN_DIR).

set -eu

BIN_DIR="${BIN_DIR:-/opt/ce/bin}"
cd "$BIN_DIR"

# .env is a shell-compatible KEY=VALUE file (plus comments).
if [ -f ./.env ]; then
    # shellcheck disable=SC1091
    . ./.env
fi

DATA_DIR="${DATA_DIR:-/tmp/ce/data}"
LOG_DIR="${LOG_DIR:-/tmp/ce/log}"
PID_DIR="${PID_DIR:-/tmp/ce/pid}"
SETTINGS_DIR="${SETTINGS_DIR:-/opt/ce/settings}"
CONFIG_DRIVE_PATH="${CONFIG_DRIVE_PATH:-${DATA_DIR}/configdrive}"
IKBD_VIRTUAL_DEVICES_PATH="${IKBD_VIRTUAL_DEVICES_PATH:-${DATA_DIR}/vdev}"

mkdir -p "$DATA_DIR" "$LOG_DIR" "$PID_DIR" "$SETTINGS_DIR" "$IKBD_VIRTUAL_DEVICES_PATH"

# PID files survive a container restart, and PIDs inside the container repeat,
# so a stale file would make the apps think another instance is running.
rm -f "$PID_DIR"/*.pid

# SETTINGS_DIR is a host bind-mount; keep it writable even if the host
# directory was created with restrictive permissions.
chmod a+rwx "$SETTINGS_DIR" 2>/dev/null || true

# Runtime copy of the config drive, same as shellscripts/ce_start.sh
if [ -d "$BIN_DIR/configdrive" ]; then
    rm -rf "$CONFIG_DRIVE_PATH"
    cp -a "$BIN_DIR/configdrive" "$CONFIG_DRIVE_PATH"
fi

CORE_PID=""
WEB_PID=""
stopping=0

cleanup() {
    if [ "$stopping" -eq 1 ]; then
        return
    fi
    stopping=1
    if [ -n "$CORE_PID" ]; then
        kill -INT "$CORE_PID" 2>/dev/null || true
    fi
    if [ -n "$WEB_PID" ]; then
        kill -INT "$WEB_PID" 2>/dev/null || true
    fi
    wait || true
}

trap 'cleanup; exit 0' INT TERM
trap cleanup EXIT

# Both apps write stdout + stderr to the container console, each line prefixed with the app name.
# The output goes through a FIFO, so $! is the app's PID (not the PID of the prefixing sed).
# stdbuf -oL: C stdio is line buffered only on a terminal, so without it the cores output would come late.
# sed -u: don't buffer the prefixed lines; also drop the cores' clear-screen escape codes and empty lines.
FIFO_DIR=$(mktemp -d)
mkfifo "$FIFO_DIR/cores" "$FIFO_DIR/web"
sed -u -e 's/\x1b\[[0-9;]*[HJ]//g' -e '/^\s*$/d' -e 's/^/[cores] /' <"$FIFO_DIR/cores" &
sed -u -e 's/^/[web]   /' <"$FIFO_DIR/web" &

stdbuf -oL -eL ./ce_cores.elf >"$FIFO_DIR/cores" 2>&1 &
CORE_PID=$!

./webserver >"$FIFO_DIR/web" 2>&1 &
WEB_PID=$!

# Stay up while both processes are running; stop the other if one exits.
while kill -0 "$CORE_PID" 2>/dev/null && kill -0 "$WEB_PID" 2>/dev/null; do
    sleep 1
done

exit 1
