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

PATH=$PATH:/ce/update     # add our update folder to path so we can use relative paths when referencing our scripts

#--------------------------
# check what chips we really need to flash

hans_curr=$( read_from_file /ce/update/hans.current 0 )
franz_curr=$( read_from_file /ce/update/franz.current 0 )
xilinx_curr=$( read_from_file /ce/update/xilinx.current 0 )

hans_new=$( read_from_file /ce/update/hans.version 1 )
franz_new=$( read_from_file /ce/update/franz.version 1 )
xilinx_new=$( read_from_file /ce/update/xilinx_used.version 1 ) # get new version for last used xilinx type by using this symlink

# turn trigger files into variables and delete those files
CE_FLASH_ALL=$( getdotenv.sh CE_FLASH_ALL "" )
CE_FLASH_XILINX=$( getdotenv.sh CE_FLASH_XILINX "" )
[ -f "$CE_FLASH_ALL" ]    && flash_all=1    || flash_all=0
[ -f "$CE_FLASH_XILINX" ] && flash_xilinx=1 || flash_xilinx=0
echo "should flash all     : $flash_all"
echo "should flash Xilinx  : $flash_xilinx"
rm -f "$CE_FLASH_XILINX" "$CE_FLASH_ALL"    # delete flash forcing files

# flash Hans if got different version of should flash all
([ "$hans_new" != "$hans_curr" ] || [ "$flash_all" = "1" ]) && update_hans=1 || update_hans=0
echo "should flash Hans    : $update_hans (new: $hans_new, current: $hans_curr)"

# flash Franz if got different version of should flash all
([ "$franz_new" != "$franz_curr" ] || [ "$flash_all" = "1" ]) && update_franz=1 || update_franz=0
echo "should flash Franz   : $update_franz (new: $franz_new, current: $franz_curr)"

# flash Xilinx if got different version of should flash all or should flash xilinx
([ "$xilinx_new" != "$xilinx_curr" ] || [ "$flash_xilinx" = "1" ] || [ "$flash_all" = "1" ]) && update_xilinx=1 || update_xilinx=0
echo "should flash Xilinx  : $update_xilinx (new: $xilinx_new, current: $xilinx_curr)"

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
[ "$mm" = "HWFWMM: MISMATCH" ] && echo "Will now reflash Xilinx again due to HW vs FW mismatch" && /ce/update/update_xilinx.sh

echo "flash_chips          : END"
