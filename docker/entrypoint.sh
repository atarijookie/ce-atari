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

# SETTINGS_DIR is a host bind-mount; keep it writable even if the host
# directory was created with restrictive permissions.
chmod a+rwx "$SETTINGS_DIR" 2>/dev/null || true

# Runtime copy of the config drive, same as shellscripts/ce_start.sh
if [ -d "$BIN_DIR/configdrive" ]; then
    rm -rf "$CONFIG_DRIVE_PATH"
    cp -a "$BIN_DIR/configdrive" "$CONFIG_DRIVE_PATH"
fi

# Web UI authenticates via PAM "login". Create/update a user when requested.
if [ -n "${CE_WEB_USER:-}" ] && [ -n "${CE_WEB_PASSWORD:-}" ]; then
    if ! id "$CE_WEB_USER" >/dev/null 2>&1; then
        useradd --create-home --shell /usr/sbin/nologin "$CE_WEB_USER"
    fi
    echo "$CE_WEB_USER:$CE_WEB_PASSWORD" | chpasswd
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

./ce_cores.elf &
CORE_PID=$!

./webserver &
WEB_PID=$!

# Stay up while both processes are running; stop the other if one exits.
while kill -0 "$CORE_PID" 2>/dev/null && kill -0 "$WEB_PID" 2>/dev/null; do
    sleep 1
done

exit 1
