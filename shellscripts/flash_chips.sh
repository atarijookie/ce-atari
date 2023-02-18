#!/bin/sh

echo "flash_chips          : START"

# This script will compare current and new version of chips firmware versions
# and do the chips flashing if needed.
# It will be run:
# - during the full update
# - before normal ce_main_app start, so the chips will be up-to-date with that app needs (e.g. after new SD card image has been written)

read_from_file()
{
  # $1 - path to file
  # $2 - default value if file not exists
  [ -f "$1" ] && cat $1 || echo $2
}

echo "adding X permissions : *.sh, *.elf"
find /ce/ -type f \( -name "*.sh" -o -name "*.elf" \) -exec chmod +x {} \;

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

# flash Hans if got different version of should flash all
if [ "$hans_new" != "$hans_curr" ] || [ -f /tmp/FW_FLASH_ALL ]; then
    update_hans=1
    echo "should flash Hans    : yes"
else
    echo "should flash Hans    : no"
fi

# flash Franz if got different version of should flash all
if [ "$franz_new" != "$franz_curr" ] || [ -f /tmp/FW_FLASH_ALL ]; then
    update_franz=1
    echo "should flash Franz   : yes"
else
    echo "should flash Franz   : no"
fi

# flash Xilinx if got different version of should flash all or should flash xilinx
if [ "$xilinx_new" != "$xilinx_curr" ] || [ -f /tmp/FW_FLASH_XILINX ] || [ -f /tmp/FW_FLASH_ALL ]; then
    update_xilinx=1
    echo "should flash Xilinx  : yes"
else
    echo "should flash Xilinx  : no"
fi

rm -f /tmp/FW_FLASH_XILINX /tmp/FW_FLASH_ALL    # delete flash forcing files

#--------------------------
# do the actual chip flashing

# update xilinx
[ "$update_xilinx" != "0" ] && /ce/update/flash_xilinx.sh

# update Hans, copy version the file to CURRENT so we'll know what we have flashed
[ "$update_hans" != "0" ] && /ce/update/flash_stm32.sh 0 && cp -f /ce/update/hans.version /ce/update/hans.current

# update Franz, copy version the file to CURRENT so we'll know what we have flashed
[ "$update_franz" != "0" ] && /ce/update/flash_stm32.sh 1 && cp -f /ce/update/franz.version /ce/update/franz.current

#--------------
# check if it's now HW vs FW mismatch, which might need another Xilinx flashing
out=$( /ce/services/core/cosmosex.elf hwinfo )
mm=$( echo "$out" | grep 'HWFWMM' )

# if MISMATCH detected, flash xilinx again -- without programmed Hans the HW reporting from app could be wrong, and thus this one is needed to fix the situation
[ "$mm" = "HWFWMM: MISMATCH" ] && /ce/update/update_xilinx.sh

echo "flash_chips          : END"
