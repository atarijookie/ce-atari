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
chmod +x /ce/update/flash_stm32_2021
chmod +x /ce/update/flash_xilinx
chmod +x /ce/*.sh
chmod +x /ce/update/*.sh

#--------------------------
# check what chips we really need to flash

horst_curr=$(  read_from_file /ce/update/horst.current       0 )
hans_curr=$(   read_from_file /ce/update/hans.current        0 )
franz_curr=$(  read_from_file /ce/update/franz.current       0 )
xilinx_curr=$( read_from_file /ce/update/xilinx.current      0 )

horst_new=$(   read_from_file /ce/update/horst.version       1 )
hans_new=$(    read_from_file /ce/update/hans.version        1 )
franz_new=$(   read_from_file /ce/update/franz.version       1 )
xilinx_new=$(  read_from_file /ce/update/xilinx_used.version 1 ) # get new version for last used xilinx type by using this symlink

update_horst=$(  [ "$horst_new"  != "$horst_curr"  ] && echo "1" || echo "0" )
update_hans=$(   [ "$hans_new"   != "$hans_curr"   ] && echo "1" || echo "0" )
update_franz=$(  [ "$franz_new"  != "$franz_curr"  ] && echo "1" || echo "0" )
update_xilinx=$( [ "$xilinx_new" != "$xilinx_curr" ] && echo "1" || echo "0" )

# check if forcing flash all
if [ -f /tmp/FW_FLASH_ALL ]; then       # if we're forcing to flash all chips (e.g. on new device)
    rm -f /tmp/FW_FLASH_ALL             # delete file so we won't force flash all next time
    update_horst=1
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
# find out which HW version we're running
# If we're not running v1 or v2, we can skip flashing Franz and Xilinx, as they are not present in v3.
hw_ver=$( /ce/whichhw.sh )

if [ "$hw_ver" -eq "3" ]; then           # on v3 hardware don't flash Franz and Xilinx (they are not there!)
    update_franz=0
    update_xilinx=0
fi

#--------------------------
# do the actual chip flashing

# update xilinx
if [ "$update_xilinx" -gt "0" ]; then
    /ce/update/update_xilinx.sh
fi

# update Hans or Horst
if [ "$hw_ver" -eq "3" ]; then              # on HW v3 - update Horst if needed
    if [ "$update_horst" -gt "0" ]; then    # should update Horst?
        /ce/update/update_hans.sh           # run same script as for update of Hans, it will use the right .hex file
        cp -f /ce/update/horst.version /ce/update/horst.current     # copy version the file to CURRENT so we'll know what we have flashed
    else
        printf "Flashing of Horst not needed.\n"
    fi
else                                        # on HW v1 and v2 - update Hans if needed
    if [ "$update_hans" -gt "0" ]; then     # should update Hans?
        /ce/update/update_hans.sh
        cp -f /ce/update/hans.version /ce/update/hans.current       # copy version the file to CURRENT so we'll know what we have flashed
    else
        printf "Flashing of Hans not needed.\n"
    fi
fi

# update franz
if [ "$update_franz" -gt "0" ]; then
    /ce/update/update_franz.sh
    cp -f /ce/update/franz.version /ce/update/franz.current   # copy version the file to CURRENT so we'll know what we have flashed
fi

if [ "$update_xilinx" -eq "0" ] && [ "$update_franz" -eq "0" ] && [ "$update_horst" -eq "0" ] && [ "$update_hans" -eq "0" ]; then
    printf "No chip flashing seems to be needed.\n"
fi

#--------------
if [ "$hw_ver" -eq "3" ]; then          # on v3 hardware there is no Xilinx, so we can skip the last part which resolves the Xilinx FW mismatch
    exit 0
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

printf "\ncheck_and_flash_chips.sh finished\n"
