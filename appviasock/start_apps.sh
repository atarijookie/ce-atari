#!/bin/sh

if [ $(id -u) != 0 ]; then
  echo "Please run this as root"
  exit
fi

# dirs and files definitions
. /ce/services/.env              # source env variables
PID_FILE="${DATA_DIR}/start_apps.pid"

mkdir -p "$DATA_DIR"             # create var dir if not exists

# output PID to file
echo $$ > $PID_FILE

# start the apps and forward them to socket
echo "Starting apps."
./appviasock "${DATA_DIR}/app0.sock" /ce/ce_conf.sh > /dev/null 2>&1 &
./appviasock "${DATA_DIR}/app1.sock" /ce/ce_fdd.sh > /dev/null 2>&1 &
./appviasock "${DATA_DIR}/app2.sock" term > /dev/null 2>&1 &

# TODO: start optional console apps (e.g. from installed extensions)

echo "Apps started."

# store description of started apps to file
echo -e "CE config tool\nCE floppy config\nlinux shell" > "${DATA_DIR}/apps.txt"

# remove PID file at the end
rm -f $PID_FILE
