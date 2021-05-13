#!/bin/sh
# Hans/Horst FW update

hw_ver=$( /ce/whichhw.sh )
dev_name=$( [ "$hw_ver" -eq "3" ] && echo "Horst" || echo "Hans" )

printf "\n----------------------------------\n>>> Updating $dev_name - START\n"

# if symlink serial0 exists, use it, otherwise try to use ttyAMA0
serialport=$( [ -e /dev/serial0 ] && echo "/dev/serial0" || echo "/dev/ttyAMA0" )
printf "Will use serial port: $serialport\n"

if [ "$hw_ver" -eq "3" ]; then      # for for v3 - we got Horst
    /ce/update/flash_stm32_2021 -x -w /ce/update/horst.hex $serialport || exit 1
else                                # for v1 and v2 - we got Hans
    /ce/update/flash_stm32 -x -w /ce/update/hans.hex $serialport || exit 1
fi

printf "\n>>> Updating $dev_name - END\n----------------------------------\n"
