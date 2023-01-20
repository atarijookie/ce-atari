#!/bin/sh
# Franz FW update

echo "----------------------------------"
echo " "
echo ">>> Updating Franz - START"

#----------------------------------------
# if symlink serial0 exists, use it, otherwise try to use ttyAMA0
if [ -f /dev/serial0 ]; then
    serialport="/dev/serial0"
else
    serialport="/dev/ttyAMA0"
fi

echo "Will use serial port: " $serialport
#----------------------------------------

/ce/update/flash_stm32 -y -w /ce/update/franz.hex $serialport

echo " "
echo ">>> Updating Franz - END"
echo "----------------------------------"

