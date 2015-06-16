#!/bin/sh
# Hans FW update

echo "----------------------------------"
echo -e "\n>>> Updating Hans - START"

if [ ! -f /tmp/hans.hex ]; then                 # if this file doesn't exist, try to extract it from ZIP package
    if [ -f /tmp/ce_update.zip ]; then      # got the ZIP package, unzip
        unzip -o /tmp/ce_update.zip -d /tmp
    else                                    # no ZIP package? damn!
        echo "/tmp/hans.hex and /tmp/ce_update.zip don't exist, can't update!"
        exit
    fi
fi

/ce/update/flash_stm32 -x -w /tmp/hans.hex /dev/ttyAMA0
rm -f /tmp/hans.hex
echo -e "\n>>> Updating Hans - END"
echo "----------------------------------"
