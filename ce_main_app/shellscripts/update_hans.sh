#!/bin/sh
# Hans FW update

echo "----------------------------------"
echo " "
echo ">>> Updating Hans - START"

#----------------------------------------
# if symlink serial0 exists, use it, otherwise try to use ttyAMA0
if [ -f /dev/serial0 ]; then
    serialport="/dev/serial0"
else
    serialport="/dev/ttyAMA0"
fi

echo "Will use serial port: " $serialport
#----------------------------------------

/ce/update/flash_stm32 -x -w /ce/update/hans.hex $serialport || exit 1

echo " "
echo ">>> Updating Hans - END"
echo "----------------------------------"

