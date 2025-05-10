#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

# source .env file to get all the settings into env vars
source .env

# create the log and data dirs if they don't exist
mkdir -p "$DATA_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$SETTINGS_DIR"

# make a copy of config drive into the expected destination 
cp -rf ./configdrive $CONFIG_DRIVE_PATH

# run the services
./ce_discovery.elf &
