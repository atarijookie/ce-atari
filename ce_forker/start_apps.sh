#!/bin/sh

if [ $(id -u) != 0 ]; then
  echo "Please run this as root"
  exit
fi

# dirs and files definitions
VAR_DIR="/var/run/ce"
PID_FILE="$VAR_DIR/start_apps.pid"

mkdir -p "$VAR_DIR"             # create var dir if not exists

# make sure this script still isn't running from the previous run
if [ -f "$PID_FILE" ]; then             # if pid file exists
    current_pid=$( cat "$PID_FILE" )

    if [ -n "$(ps -p $current_pid -o pid=)" ]; then         # if the PID is still running
        echo "Instance of this script is still running, not running again."
        exit
    fi
fi

# output PID to file
echo $$ > $PID_FILE

# start the apps and forward them to socket
echo "Starting apps."
./appviasock "$VAR_DIR/app0.sock" /ce/ce_conf.sh > /dev/null 2>&1 &
./appviasock "$VAR_DIR/app1.sock" /ce/ce_fdd.sh > /dev/null 2>&1 &
./appviasock "$VAR_DIR/app2.sock" term > /dev/null 2>&1 &

# TODO: start optional console apps (e.g. from installed extensions)

echo "Apps started."

# store description of started apps to file
echo -e "CE config tool\nCE floppy config\nlinux shell" > "$VAR_DIR/apps.txt"

# remove PID file at the end
rm -f $PID_FILE
