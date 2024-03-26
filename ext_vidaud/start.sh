#!/bin/sh
#
# This script is used to start extension and store its PID into file.
#
# params: 
#   $1 - path to unix socket of CE, where the responses should be sent
#   $2 - extension id - id (index) under which the CE core has this extension stored
#

EXT_NAME=ext_vidaud         # <<< CHANGE THIS IN YOUR OWN EXTENSION TO SOMETHING ELSE
PID_FILE=/tmp/${EXT_NAME}.pid

# Find out where this start script it located and change to that directory.
# We need to do this because if this script is called from CE core or TaskQ, it may be
# having different current directory and then finding stop script or executable won't work.
EXT_DIR=$( realpath $( dirname $0 ) )
echo "Extension running in dir: $EXT_DIR"
cd $EXT_DIR

# stop this extension if already running and now being instructed to start again
./stop.sh

EXECUTABLE="./${EXT_NAME}.elf"

# if the executable doesn't exist, run the compilation
if [ ! -f "${EXECUTABLE}" ]; then
    ./build.sh
fi

# start the extension - detached, so it doesn't block this start script from finishing
EXEC_CMD="${EXECUTABLE} $1 $2"
eval $EXEC_CMD &

# store PID to some file for usage in stop.sh
EXT_PID=$!

sleep 1     # give extension short time to start and if it fails this soon, just don't store PID in file

if ps -p $EXT_PID > /dev/null 2>&1; then        # pid running?
    echo "Extension started - has PID " $EXT_PID
    echo $EXT_PID > $PID_FILE
else
    echo "Extension failed to start - no running PID " $EXT_PID
fi