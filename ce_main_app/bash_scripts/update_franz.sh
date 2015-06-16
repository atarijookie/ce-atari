#!/bin/sh
# Franz FW update

echo "----------------------------------"
echo -e "\n>>> Updating Franz - START"

if [ ! -f /tmp/franz.hex ]; then            # if this file doesn't exist, try to extract it from ZIP package
    if [ -f /tmp/ce_update.zip ]; then      # got the ZIP package, unzip
        unzip -o /tmp/ce_update.zip -d /tmp
    else                                    # no ZIP package? damn!
        echo "/tmp/franz.hex and /tmp/ce_update.zip don't exist, can't update!"
        exit
    fi
fi

/ce/update/flash_stm32 -y -w /tmp/franz.hex /dev/ttyAMA0
rm -f /tmp/franz.hex
echo -e "\n>>> Updating Franz - END"
echo "----------------------------------"
