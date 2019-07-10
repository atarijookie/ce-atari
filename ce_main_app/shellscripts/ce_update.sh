#!/bin/sh

# first stop any cosmosex process (script or app)
/ce/ce_stop.sh

echo " "
echo "Updating CosmosEx from internet, this will take a while."
echo "DO NOT POWER OFF THE DEVICE!!!"
echo " "

distro=$( /ce/whichdistro.sh )

# get changed files from repo
git pull 

# update xilinx
/ce/update/update_xilinx.sh

# update hans
/ce/update/update_hans.sh

# update franz
/ce/update/update_franz.sh

#--------------
# check if it's now HW vs FW mismatch, which might need another Xilinx flashing
out=$( /ce/app/cosmosex hwinfo )
mm=$( echo "$out" | grep 'HWFWMM' )

# if MISMATCH detected, flash xilinx again -- without programmed Hans the HW reporting from app could be wrong, and thus this one is needed to fix the situation
if [ "$mm" = "HWFWMM: MISMATCH" ]; then
    /ce/update/update_xilinx.sh
fi 
#--------------

# on jessie update SysV init script, remove systemctl service
if [ "$distro" = "raspbian_jessie" ]; then
    cp "/ce/initd_cosmosex" "/etc/init.d/cosmosex"
    rm -f "/etc/systemd/system/cosmosex.service"
fi

# on stretch remove SysV init script, update systemctl service
if [ "$distro" = "raspbian_stretch" ]; then
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
    echo "No core_freq in /boot/config.txt, adding it"
    addCoreFreq=1
fi

# B) more than one core_freq? Remove them, then add one
if [ "$coreFreqCountAny" -gt "1" ]; then
    echo "Too many core_freq in /boot/config.txt, fixing it"
    addCoreFreq=1
fi

# C) There is some core_freq, but it's not correct? Remove it, then add correct one
if [ "$coreFreqCountAny" -gt "0" ] && [ "$coreFreqCountCorrect" -eq "0" ]; then
    echo "core_freq in /boot/config.txt is incorrect, fixing it"
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
    echo "core_freq is ok in /boot/config.txt"
fi

#------------------------
# disable ctrl-alt-del causing restart
ln -fs /dev/null /lib/systemd/system/ctrl-alt-del.target

# disable auto-login
ln -fs /lib/systemd/system/getty@.service /etc/systemd/system/getty.target.wants/getty@tty1.service

sync

echo " "
echo "Update done, you may start the /ce/ce_start.sh now!";
echo " "


