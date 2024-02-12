#!/bin/sh
#
# This script is used to start extension and store its PID into file.
#
# params: 
#   $1 - path to unix socket of CE, where the responses should be sent
#

# stop this extension if already running and now being instructed to start again
./stop.sh

# start the extension - detached, so it doesn't block this start script from finishing
python3 main.py $1 &

# store PID to some file for usage in stop.sh
EXT_PID=$!

sleep 1     # give extension short time to start and if it fails this soon, just don't store PID in file

if ps -p $EXT_PID > /dev/null 2>&1; then        # pid running?
    echo "Extension started - has PID " $EXT_PID
    echo $EXT_PID > /tmp/extension_example.pid
else
    echo "Extension failed to start - no running PID " $EXT_PID
fi
