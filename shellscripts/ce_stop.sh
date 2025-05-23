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
  kill -2 "$pid" 2>/dev/null    # SIGINT
  sleep 0.3
  kill -9 "$pid" 2>/dev/null    # SIGKILL
  echo "Killed process $pid from $pidfile"

  rm -f "$pidfile"

done

# delete all sock files that might be present
rm -f $DATA_DIR/*sock*

echo "All processes terminated."
