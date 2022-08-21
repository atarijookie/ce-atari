#!/bin/sh

#----------------------------------------
# if symlink serial0 exists, use it, otherwise try to use ttyAMA0
serialport=$( [ -e /dev/serial0 ] && echo "/dev/serial0" || echo "/dev/ttyAMA0" )

# check if H730 mcu can be detected, and if it can, we got Horst, so no Franz is present
gotHorst=$( /ce/update/flash_stm32_2021 -x $serialport | grep "73xxx" | wc -l )

if [ "$gotHorst" -gt "0" ]; then    # if Horst was detected, it's v3
    echo "3"
    exit 0
fi

#----------------------------------------

# test for XC9536 chip
/ce/update/flash_xilinx /ce/update/test_xc9536xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then       # XC9536XL found, it's v1
    echo "1"
    exit 0
fi

# test for XC9572 chip
/ce/update/flash_xilinx /ce/update/test_xc9572xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then       # XC9572XL found, it's v2
    echo "2"
    exit 0
fi

# if nothing found, return something else
echo "0"
