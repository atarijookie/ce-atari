#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

# stop anything that is still running
./ce_stop.sh

# source .env file to get all the settings into env vars
. ./.env

# create the log and data dirs if they don't exist
mkdir -p "$DATA_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$SETTINGS_DIR"
mkdir -p "$PID_DIR"

# make a copy of config drive into the expected destination 
cp -rf "${BIN_DIR}/configdrive" "$CONFIG_DRIVE_PATH"

echo "Starting processes."

# run the services
./ce_discovery.elf > /dev/null 2>&1 &

cd webserver
./ce_web.sh > /dev/null 2>&1 &
cd - > /dev/null 2>&1

echo "Processes have been started."
