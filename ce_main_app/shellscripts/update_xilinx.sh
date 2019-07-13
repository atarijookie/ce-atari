#!/bin/sh

echo "XILINX firmware writing script."

# initialize variables
is36=0
is72=0

# test for XC9536 chip
/ce/update/flash_xilinx /ce/update/test_xc9536xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then
    is36=1
fi

# test for XC9572 chip
/ce/update/flash_xilinx /ce/update/test_xc9572xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then
    is72=1
fi

# no chip detected? fail, quit
if [ "$is36" -eq "0" ] && [ "$is72" -eq "0" ]; then
    echo "No Xilinx type detected - this is invalid!"
    exit
fi

# both chips detected? fail, quit
if [ "$is36" -eq "1" ] && [ "$is72" -eq "1" ]; then
    echo "Both types of Xilinx detected - this is invalid!"
    exit
fi

# for XC9536 - just burn the firmware
if [ "$is36" -eq "1" ]; then
    # write the XC9536 firmware
    echo "Detected XC9536 chip, will write firmware"
    /ce/update/flash_xilinx /ce/update/xilinx.xsvf
    cp /ce/update/xilinx.version /ce/update/xilinx.current          # copy flashed version into current version file
    ln -fs /ce/update/xilinx.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
    exit
fi

# for XC9572 - first check the HDD IF - if it's SCSI or ACSI
if [ "$is72" -eq "1" ]; then
    echo "Detected XC9572 chip, now will detect if it's ACSI or SCSI"
    out=$( /ce/app/cosmosex hwinfo )

    isAcsi=$( echo "$out" | grep 'ACSI' )
    isScsi=$( echo "$out" | grep 'SCSI' )

    # if it's ACSI version
    if [ -n "$isAcsi" ]; then
        echo "Detected XC9572 chip and ACSI interface, will write firmware"
        /ce/update/flash_xilinx /ce/update/xlnx2a.xsvf
        cp /ce/update/xlnx2a.version /ce/update/xilinx.current          # copy flashed version into current version file
        ln -fs /ce/update/xlnx2a.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
        exit
    fi

    # if it's SCSI version
    if [ -n "$isScsi" ]; then
        echo "Detected XC9572 chip and SCSI interface, will write firmware"
        /ce/update/flash_xilinx /ce/update/xlnx2s.xsvf
        cp /ce/update/xlnx2s.version /ce/update/xilinx.current          # copy flashed version into current version file
        ln -fs /ce/update/xlnx2s.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
        exit
    fi

    echo "Detected XC9572 chip but didn't write any firmware :("
fi

