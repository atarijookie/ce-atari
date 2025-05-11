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

cd libdospath
echo "building libdospath"
make -j4
cd - > /dev/null 2>&1

cd ce_discovery
echo "building ce_discovery"
make -j4
cp ./ce_discovery.elf "$BIN_DIR"
cd - > /dev/null 2>&1

cd ce_core_hdd
echo "building ce_core_hdd"
make -j4
cp ./ce_hdd.elf "$BIN_DIR"
cp -r ./configdrive "$BIN_DIR"
cd - > /dev/null 2>&1

# cd ce_core_fdd
# echo "building ce_core_fdd"
# make -j4
# cp ./ce_fdd.elf "$BIN_DIR"
# cd - > /dev/null 2>&1

# cd ce_core_ikbd
# echo "building ce_core_ikbd"
# make -j4
# cp ./ce_ikbd.elf "$BIN_DIR"
# cd - > /dev/null 2>&1

echo "copying webserver"
cp -r ./webserver "$BIN_DIR"

echo "copying .env file"
cp ./.env "$BIN_DIR"
ln -sf "${BIN_DIR}/.env" "${BIN_DIR}/webserver/.env"

echo "copying shell scripts"
cp ./shellscripts/ce_start.sh "$BIN_DIR"
cp ./shellscripts/ce_stop.sh "$BIN_DIR"

echo "done"
