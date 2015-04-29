#!/bin/sh

# Do Hans FW update
# requires: existing /tmp/hans.hex

echo "----------------------------------"
echo -e "\n>>> Updating Hans - START"
/ce/update/flash_stm32 -x -w /tmp/hans.hex /dev/ttyAMA0
rm -f /tmp/hans.hex
echo -e "\n>>> Updating Hans - END"
echo "----------------------------------"
