#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

# source .env file to get all the settings into env vars
. ./.env

# create pid dir if it doesn't exist
mkdir -p "$PID_DIR"

echo "Terminating running processes."

# Find all *.pid files
for pidfile in $( find "${PID_DIR}" -name *.pid ); do
  # Read PID from file
  pid=$(cat "$pidfile" 2>/dev/null)

  # Kill the process
  if kill -9 "$pid" 2>/dev/null; then
    echo "Killed process $pid from $pidfile"
  else
    echo "Failed to kill PID $pid from $pidfile (maybe not running)"
  fi

  rm -f "$pidfile"

done

echo "All processes terminated."
