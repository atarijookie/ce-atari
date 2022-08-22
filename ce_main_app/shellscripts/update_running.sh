#!/bin/sh

VAR_DIR="/var/run/ce"
PID_FILE="$VAR_DIR/update.pid"

mkdir -p "$VAR_DIR"                     # create var dir if not exists

# check if the script with PID in pid file is running or not
if [ -f "$PID_FILE" ]; then             # if pid file exists
    current_pid=$( cat "$PID_FILE" )

    if [ -n "$(ps -p $current_pid -o pid=)" ]; then     # if the PID is still running
        exit 1      # is running
    else
        exit 0      # not running
    fi
fi

# if got here, pid file doesn't exist and script is thus not running
exit 0
