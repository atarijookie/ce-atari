#!/bin/sh

[ ! -f /ce/services/.env ] && echo ".env file not found!" && exit 1
. /ce/services/.env       # source env variables

echo "flash_chips          : START"

# This script will compare current and new version of chips firmware versions
# and do the chips flashing if needed.
# It will be run:
# - during the full update
# - before normal ce_main_app start, so the chips will be up-to-date with that app needs (e.g. after new SD card image has been written)

echoerr() { echo "$@" 1>&2; }

read_from_file()
{
  # $1 - path to file
  # $2 - default value if file not exists
  [ -f "$1" ] && cat $1 || echo $2
}

get_chip_interface()
{
  unset ci
  . /etc/ce.env 2> /dev/null    # try to source this env file, which could hold 'ci=4' for chip interface v4

  [ "$ci" = "4" ] && echo "4" && return    # if ci=4, we can return 4 and return

  echo "1"    # in other cases just assume chip interface v1/v2
}

should_update_chip()
{
  # $1 - chip name - e.g. hans / franz / xilinx
  # $2 - flash_all - if '1', then should flash all chips
  # $3 - flash_xilinx - additional flag for just xilinx flashing

  ver_curr=0
  ver_new=0

  ci=$( get_chip_interface )

  # if this is chip interface v4, device has only Franz, and we're not asking for Franz, so don't update...
  if [ "$ci" = "4" ] && [ "$1" != "franz" ]; then
    update=0
  else      # For v4 and Franz OR v1 and any chip the rest logic applies
    # for xilinx chip use this postfix in the new version file
    [ "$1" = "xilinx" ] && postfix='_used' || postfix=''

    ver_curr=$( read_from_file /ce/update/$1.current 0 )
    ver_new=$( read_from_file /ce/update/${1}${postfix}.version 1 )

    # if flash-all or flash xilinx, then flash this
    if [ "$2" = "1" ] || [ "$3" = "1" ]; then
      update=1
    else  # not flash_all and not flash_xilinx, should flash if got different version
      [ "$ver_new" != "$ver_curr" ] && update=1 || update=0
    fi
  fi

  printf "should flash %-8s: $update (new: $ver_new, current: $ver_curr)\n" "$1" 1>&2
  echo "$update"
}

echo "adding X permissions : *.sh, *.elf"
find /ce/ -type f \( -name "*.sh" -o -name "*.elf" \) -exec chmod +x {} \;

PATH=$PATH:/ce/update     # add our update folder to path so we can use relative paths when referencing our scripts

#--------------------------
# check what chips we really need to flash

# turn trigger files into variables and delete those files
[ -f "$CE_FLASH_ALL" ]    && flash_all=1    || flash_all=0
[ -f "$CE_FLASH_XILINX" ] && flash_xilinx=1 || flash_xilinx=0
echo "should flash all     : $flash_all"
echo "should flash Xilinx  : $flash_xilinx"
rm -f "$CE_FLASH_XILINX" "$CE_FLASH_ALL"    # delete flash forcing files

update_hans=$( should_update_chip "hans" "$flash_all" "0" )                   # flash Hans if got different version of should flash all
update_franz=$( should_update_chip "franz" "$flash_all" "0" )                 # flash Franz if got different version of should flash all
update_xilinx=$( should_update_chip "xilinx" "$flash_all" "$flash_xilinx" )   # flash Xilinx if got different version of should flash all or should flash xilinx

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
