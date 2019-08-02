#!/bin/sh

# This script will compare current and new version of chips firmware versions
# and do the chips flashing if needed.
# It will be run:
# - during the full update
# - before normal ce_main_app start, so the chips will be up-to-date with that app needs (e.g. after new SD card image has been written)

read_from_file()
{
    if [ -f $1 ]; then      # does the file exist? read value from file
        val=$( cat $1 )
    else                    # file doesn't exist? use default value
        val=$2
    fi

    echo $val               # display the value
}

#--------------------------
# add execute permissions to scripts and binaries (if they don't have them yet)
chmod +x /ce/app/cosmosex
chmod +x /ce/update/flash_stm32
chmod +x /ce/update/flash_xilinx
chmod +x /ce/*.sh
chmod +x /ce/update/*.sh

#--------------------------
# check what chips we really need to flash

hans_curr=$( read_from_file /ce/update/hans.current 0 )
franz_curr=$( read_from_file /ce/update/franz.current 0 )
xilinx_curr=$( read_from_file /ce/update/xilinx.current 0 )

hans_new=$( read_from_file /ce/update/hans.version 1 )
franz_new=$( read_from_file /ce/update/franz.version 1 )
xilinx_new=$( read_from_file /ce/update/xilinx_used.version 1 ) # get new version for last used xilinx type by using this symlink

update_hans=0
update_franz=0
update_xilinx=0

# check if Hans has new FW available
if [ "$hans_new" != "$hans_curr" ]; then           # got different FW than current? do update (don't check for newer only, as someone might want to use older version)
    update_hans=1
fi

# check if Franz has new FW available
if [ "$franz_new" != "$franz_curr" ]; then         # got different FW than current? do update (don't check for newer only, as someone might want to use older version)
    update_franz=1
fi

# check if Xilinx has new FW available
if [ "$xilinx_new" != "$xilinx_curr" ]; then       # got different FW than current? do update (don't check for newer only, as someone might want to use older version)
    update_xilinx=1
fi

# check if forcing flash all
if [ -f /tmp/FW_FLASH_ALL ]; then       # if we're forcing to flash all chips (e.g. on new device)
    rm -f /tmp/FW_FLASH_ALL             # delete file so we won't force flash all next time
    update_hans=1
    update_franz=1
    update_xilinx=1
fi

# check if forcing xilinx re-flash
if [ -f /tmp/FW_FLASH_XILINX ]; then    # if we're forcing to flash Xilinx (e.g. on SCSI / ACSI interface change )
    rm -f /tmp/FW_FLASH_XILINX          # delete file so we won't force flash all next time
    update_xilinx=1
fi

#--------------------------
# do the actual chip flashing

# update xilinx
if [ "$update_xilinx" -gt "0" ]; then
    /ce/update/update_xilinx.sh
fi

# update hans
if [ "$update_hans" -gt "0" ]; then
    /ce/update/update_hans.sh
    cp -f /ce/update/hans.version /ce/update/hans.current     # copy version the file to CURRENT so we'll know what we have flashed
fi

# update franz
if [ "$update_franz" -gt "0" ]; then
    /ce/update/update_franz.sh
    cp -f /ce/update/franz.version /ce/update/franz.current   # copy version the file to CURRENT so we'll know what we have flashed
fi

#--------------
# check if it's now HW vs FW mismatch, which might need another Xilinx flashing
out=$( /ce/app/cosmosex hwinfo )
mm=$( echo "$out" | grep 'HWFWMM' )

# if MISMATCH detected, flash xilinx again -- without programmed Hans the HW reporting from app could be wrong, and thus this one is needed to fix the situation
if [ "$mm" = "HWFWMM: MISMATCH" ]; then
    /ce/update/update_xilinx.sh
fi
#--------------

echo "check_and_flash_chips.sh finished"
