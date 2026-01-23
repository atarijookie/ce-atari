#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

set -e

# source .env file to get all the settings into env vars
. ./.env

# create the log and data dirs if they don't exist
mkdir -p "$DATA_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$SETTINGS_DIR"
mkdir -p "$PID_DIR"
mkdir -p "$BIN_DIR"

cd libdospath
echo "building libdospath"
make -j4
cd - > /dev/null 2>&1

cd ce_cores
echo "building ce_cores"
make -j4
cp ./ce_cores.elf "$BIN_DIR"
cp ./ce_logo.bin "$BIN_DIR"
cp -r ./configdrive "$BIN_DIR"
cd - > /dev/null 2>&1

cd webserver
echo "building webserver"
go build
cp ./webserver "$BIN_DIR"
cd - > /dev/null 2>&1

echo "copying webserver pages"
cp -r ./webserver/static "$BIN_DIR"

echo "copying .env file"
cp ./.env "$BIN_DIR"

echo "copying shell scripts"
cp ./shellscripts/ce_start.sh "$BIN_DIR"
cp ./shellscripts/ce_stop.sh "$BIN_DIR"

echo "done"
