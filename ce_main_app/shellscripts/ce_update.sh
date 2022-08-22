#!/bin/sh
VAR_DIR="/var/run/ce"
PID_FILE="$VAR_DIR/update.pid"
mkdir -p "$VAR_DIR"                         # create var dir if doesn't exist

LOG_DIR="/var/log/ce"
mkdir -p "$LOG_DIR"                         # create log dir if doesn't exist
timestamp=$( date "+%Y%m%d%H%M%S" )
LOG_FILE="$LOG_DIR/update.log.$timestamp"   # generate logfile name with timestamp, so multiple runs of update can be observed

{           # start of block for output redirect
start=$( date )
printf "ce_update.sh started at : $start\n"     # show start time

# check if this update script is already running and terminate if it is
/ce/update/update_running.sh

if [ $? -ne 0 ]; then
    echo " "
    echo "The update is already / still running, not running it again."
    echo "If you want to run it again, wait until it finishes."
    
    stop=$( date )
    printf "\nce_update.sh finished at: $stop\n\n"     # show stop time
    exit
fi

# output PID to file
echo $$ > $PID_FILE

# Stop any cosmosex app, and pass 1st and 2nd argument of this script to the stop script
# When arguments are not given, ce_stop will stop not only CosmosEx app, but also cesuper.sh script, which is fine when called manually from shell by user.
# When 'nosystemctl dontkillcesuper' arguments are given, ce_stop will stop onl CosmosEx app, but not cesuper.sh script, which is what we want when ce_update.sh is called as a part of update process within the cesuper.sh script
/ce/ce_stop.sh $1 $2

printf "\n\nUpdating CosmosEx from internet or USB drive, this will take a while.\nDO NOT POWER OFF THE DEVICE!!!\n\n"

distro=$( /ce/whichdistro.sh )
url_zip="http://joo.kie.sk/cosmosex/update/$distro.zip"
url_hash="http://joo.kie.sk/cosmosex/update/$distro.hash"
path_to_tmp_update="/tmp/$distro.zip"

#---------------------------
# check if should do update from USB

if [ -f /tmp/UPDATE_FROM_USB ]; then                    # if we're doing update from USB
    printf "update from USB\n"

    path_to_usb_update=$( cat /tmp/UPDATE_FROM_USB )    # get content of file into variable
    rm -f /tmp/UPDATE_FROM_USB                          # delete file so we won't do it again next time

    unzip -o $path_to_usb_update -d /ce                 # unzip update into /ce directory, overwrite without prompting
else    # download update from internet, by wget
    printf "update from internet - will use wget\n"

    rm -f $path_to_tmp_update                       # delete if file exists
    wget -O $path_to_tmp_update $url_zip            # download to /tmp/stretch.zip

    unzip -o $path_to_tmp_update -d /ce             # unzip update into /ce directory, overwrite without prompting
fi

#--------------------------
# add execute permissions to scripts and binaries (if they don't have them yet)
chmod +x /ce/app/cosmosex
chmod +x /ce/update/flash_stm32
chmod +x /ce/update/flash_stm32_2021
chmod +x /ce/update/flash_xilinx
chmod +x /ce/*.sh
chmod +x /ce/update/*.sh

#--------------------------
# check what chips we really need to flash, possibly force flash, and do the flashing

/ce/update/check_and_flash_chips.sh

#--------------

# on jessie update SysV init script, remove systemctl service
if [ "$distro" = "jessie" ]; then
    cp "/ce/initd_cosmosex" "/etc/init.d/cosmosex"
    rm -f "/etc/systemd/system/cosmosex.service"
fi

# on stretch remove SysV init script, update systemctl service
if [ "$distro" = "stretch" ]; then
    rm -f "/etc/init.d/cosmosex"
    cp "/ce/cosmosex.service" "/etc/systemd/system/cosmosex.service"
fi

#------------------------
# now add / change the core_freq param in the boot config to avoid SPI clock issues
coreFreqCountAny=$( cat /boot/config.txt | grep core_freq | wc -l )
coreFreqCountCorrect=$( cat /boot/config.txt | grep 'core_freq=250' | wc -l )

addCoreFreq=0           # don't add it yet

# A) no core_freq? Add it
if [ "$coreFreqCountAny" -eq "0" ]; then
    printf "No core_freq in /boot/config.txt, adding it\n"
    addCoreFreq=1
fi

# B) more than one core_freq? Remove them, then add one
if [ "$coreFreqCountAny" -gt "1" ]; then
    printf "Too many core_freq in /boot/config.txt, fixing it\n"
    addCoreFreq=1
fi

# C) There is some core_freq, but it's not correct? Remove it, then add correct one
if [ "$coreFreqCountAny" -gt "0" ] && [ "$coreFreqCountCorrect" -eq "0" ]; then
    printf "core_freq in /boot/config.txt is incorrect, fixing it\n"
    addCoreFreq=1
fi

# if we need to add the core_freq
if [ "$addCoreFreq" -gt "0" ]; then
    mv /boot/config.txt /boot/config.old                            # back up old
    cat /boot/config.old | grep -v 'core_freq' > /boot/config.txt   # create new without any core_freq directive

    # now append the correct core_config on the end of file
    echo "core_freq=250" >> /boot/config.txt
else
    # we don't need to do anything for case D), where there is one core_freq there and it's correct
    printf "core_freq is ok in /boot/config.txt\n"
fi

#------------------------
# disable ctrl-alt-del causing restart
ln -fs /dev/null /lib/systemd/system/ctrl-alt-del.target

# disable auto-login
ln -fs /lib/systemd/system/getty@.service /etc/systemd/system/getty.target.wants/getty@tty1.service

sync

#------------------------
# if should reboot after this (e.g. due to network settings reset), do it now
if [ -f /tmp/REBOOT_AFTER_UPDATE ]; then
    rm -f /tmp/REBOOT_AFTER_UPDATE          # delete file so we won't do it again next time
    reboot now
fi

printf "\nUpdate done, you may start the /ce/ce_start.sh now!\n";

stop=$( date )
printf "ce_update.sh finished at: $stop\n\n"     # show stop time

# remove PID file at the end
rm -f $PID_FILE

# end of block for output redirect to console and log file
} 2>&1 | tee $LOG_FILE
