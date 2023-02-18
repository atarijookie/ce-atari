#!/bin/sh

echo "update_xilinx        : START"

# initialize variables
is36=0
is72=0

# test for XC9536 chip
/ce/update/flash_xilinx.elf /ce/update/test_xc9536xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then
    is36=1
fi

# test for XC9572 chip
/ce/update/flash_xilinx.elf /ce/update/test_xc9572xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then
    is72=1
fi

# no chip detected? fail, quit
if [ "$is36" -eq "0" ] && [ "$is72" -eq "0" ]; then
    echo "xilinx type          : NONE - this is invalid"
    exit
fi

# both chips detected? fail, quit
if [ "$is36" -eq "1" ] && [ "$is72" -eq "1" ]; then
    echo "xilinx type          : BOTH - this is invalid"
    exit
fi

# for XC9536 - just burn the firmware
if [ "$is36" -eq "1" ]; then
    # write the XC9536 firmware
    echo "xilinx type          : XC9536"
    /ce/update/flash_xilinx.elf /ce/update/xilinx.xsvf
    cp /ce/update/xilinx.version /ce/update/xilinx.current          # copy flashed version into current version file
    ln -fs /ce/update/xilinx.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
    exit
fi

# for XC9572 - first check the HDD IF - if it's SCSI or ACSI
if [ "$is72" -eq "1" ]; then
    echo "xilinx type          : XC9572"
    echo "CE interface         : detecting ACSI / SCSI"
    out=$( /ce/services/core/cosmosex.elf hwinfo )

    isAcsi=$( echo "$out" | grep 'ACSI' )
    isScsi=$( echo "$out" | grep 'SCSI' )

    # if it's ACSI version
    if [ -n "$isAcsi" ]; then
        echo "CE interface         : ACSI"
        /ce/update/flash_xilinx.elf /ce/update/xlnx2a.xsvf
        cp /ce/update/xlnx2a.version /ce/update/xilinx.current          # copy flashed version into current version file
        ln -fs /ce/update/xlnx2a.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
        exit
    fi

    # if it's SCSI version
    if [ -n "$isScsi" ]; then
        echo "CE interface         : SCSI"
        /ce/update/flash_xilinx.elf /ce/update/xlnx2s.xsvf
        cp /ce/update/xlnx2s.version /ce/update/xilinx.current          # copy flashed version into current version file
        ln -fs /ce/update/xlnx2s.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
        exit
    fi

    echo "CE interface         : couldn't detect, firmware not written"
fi
