#!/bin/sh

# Do Franz FW update
# requires: existing /tmp/franz.hex

echo "----------------------------------"
echo -e "\n>>> Updating Franz - START"
/ce/update/flash_stm32 -y -w /tmp/franz.hex /dev/ttyAMA0
rm -f /tmp/franz.hex
echo -e "\n>>> Updating Franz - END"
echo "----------------------------------"
