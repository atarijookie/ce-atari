#!/bin/sh
#
# This script is used to stop running extension based on the PID stored in file.
#

PID_FILE=/tmp/extension_example.pid

if [ -f "$PID_FILE" ]; then     # if PID file exist
    PID=$( cat $PID_FILE )

    if ps -p $PID > /dev/null 2>&1; then        # pid running?
        echo "Will now terminate PID" $PID
        kill -15 $PID           # ask extension to terminate and wait a while
        sleep 1

        if ps -p $PID > /dev/null 2>&1; then    # pid STILL running?
            echo "Will force terminate PID" $PID
            kill -9 $PID            # force termination
            echo "PID now terminated."
        fi
    else
        echo "PID" $PID "not running, not terminating"
    fi
else
    echo "No PID file so assuming not running"
fi

# remove the PID file at the end
rm -f $PID_FILE
