#!/bin/sh
# Franz FW update

# find out hw version we got
hw_ver=$( /ce/whichhw.sh )

if [ "$hw_ver" -eq "3" ]; then      # if v3 was detected, we can skip flashing Franz as no Franz is present
    printf "\nThis device doesn't contain Franz, skipping flashing Franz chip.\n\n"
    exit 0                          # exit with success
fi

printf "\n----------------------------------\n>>> Updating Franz - START\n"

# if symlink serial0 exists, use it, otherwise try to use ttyAMA0
serialport=$( [ -e /dev/serial0 ] && echo "/dev/serial0" || echo "/dev/ttyAMA0" )
printf "Will use serial port: " $serialport

/ce/update/flash_stm32 -y -w /ce/update/franz.hex $serialport

printf "\n>>> Updating Franz - END\n----------------------------------\n"
