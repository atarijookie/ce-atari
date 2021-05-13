#!/bin/sh

# find out which HW version we're running
hw_ver=$( /ce/whichhw.sh )

if [ "$hw_ver" -eq "3" ]; then      # if v3 was detected, we can skip flashing Xilinx as no Xilinx is present
    printf "\nThis device doesn't contain Xilinx, skipping flashing Xilinx chip.\n"
    exit 0                          # exit with success
fi

# for XC9536 - just burn the firmware
if [ "$hw_ver" -eq "1" ]; then
    # write the XC9536 firmware
    printf "Detected XC9536 chip, will write firmware\n"

    /ce/update/flash_xilinx /ce/update/xilinx.xsvf
    cp /ce/update/xilinx.version /ce/update/xilinx.current          # copy flashed version into current version file
    ln -fs /ce/update/xilinx.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
    exit
fi

# for XC9572 - first check the HDD IF - if it's SCSI or ACSI
if [ "$hw_ver" -eq "2" ]; then
    printf "Detected XC9572 chip, now will detect if it's ACSI or SCSI\n"
    out=$( /ce/app/cosmosex hwinfo )

    isAcsi=$( echo "$out" | grep 'ACSI' )
    isScsi=$( echo "$out" | grep 'SCSI' )

    # if it's ACSI version
    if [ -n "$isAcsi" ]; then
        printf "Detected XC9572 chip and ACSI interface, will write firmware\n"
        /ce/update/flash_xilinx /ce/update/xlnx2a.xsvf
        cp /ce/update/xlnx2a.version /ce/update/xilinx.current          # copy flashed version into current version file
        ln -fs /ce/update/xlnx2a.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
        exit
    fi

    # if it's SCSI version
    if [ -n "$isScsi" ]; then
        printf "Detected XC9572 chip and SCSI interface, will write firmware\n"
        /ce/update/flash_xilinx /ce/update/xlnx2s.xsvf
        cp /ce/update/xlnx2s.version /ce/update/xilinx.current          # copy flashed version into current version file
        ln -fs /ce/update/xlnx2s.version /ce/update/xilinx_used.version # xilinx_user.version will point to file from which we took the version, so when that file changes, we will know we need to update xilinx 
        exit
    fi

    printf "Detected XC9572 chip but didn't write any firmware :(\n"
fi
