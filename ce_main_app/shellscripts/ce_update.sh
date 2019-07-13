#!/bin/sh

# first stop any cosmosex process (script or app)
/ce/ce_stop.sh

echo " "
echo "Updating CosmosEx from internet, this will take a while."
echo "DO NOT POWER OFF THE DEVICE!!!"
echo " "

distro=$( /ce/whichdistro.sh )
url_to_git_repo="https://github.com/atarijookie/ce-atari-releases.git"
path_to_repo_archive="https://github.com/atarijookie/ce-atari-releases/archive/"
path_to_zip_update="$path_to_repo_archive$distro.zip"
path_to_tmp_update="/tmp/$distro.zip"

read_from_file()
{
    if [ -f $1 ]; then      # does the file exist? read value from file
        val=$( cat $1 )
    else                    # file doesn't exist? use default value
        val=$2
    fi

    echo $val               # display the value
}

#---------------------------
# check if should do update from USB

if [ -f /tmp/UPDATE_FROM_USB ]; then                    # if we're doing update from USB
    path_to_usb_update=$( cat /tmp/UPDATE_FROM_USB )    # get content of file into variable
    rm -f /tmp/UPDATE_FROM_USB                          # delete file so we won't do it again next time

    unzip -o $path_to_usb_update -d /ce                 # unzip update into /ce directory, overwrite without prompting
else    # download update from internet, by git or wget
    # check if got git installed
    git --version

    if [ "$?" -ne "0" ]; then                           # if don't have git
        echo "Will try to install missing git"

        apt-get update
        apt-get --yes --force-yes install git           # try to install git
    fi

    # try to check for git after possible installation
    git --version

    if [ "$?" -ne "0" ]; then                           # git still missing, do it through wget
        echo "git is still missing, will use wget"

        rm -f $path_to_tmp_update                       # delete if file exists
        wget -O $path_to_tmp_update $path_to_zip_update # download to /tmp/yocto.zip

        unzip -o $path_to_tmp_update -d /ce             # unzip update into /ce directory, overwrite without prompting
    else                                                # git is present
        echo "doing git pull..."

        cd /ce/                                         # go to /ce directory

        git reset --hard origin/$distro                 # reset all tracked files so git won't complain
        git pull origin $distro                         # try git pull

        if [ "$?" -ne "0" ]; then                       # git complained that this is not a repo? as it is not empty, simple 'git clone' might fail
            echo "doing git fetch..."

            cd /ce/
            git init                                    # make this dir a repo
            git remote add origin $url_to_git_repo      # set origin to url to repo
            git fetch --depth=1                         # fetch, but only 1 commit deep
            git checkout -f $distro                     # switch to the right repo
            git pull origin $distro                     # just to be sure :)
        fi
    fi
fi

#--------------------------
# add execute permissions to scripts and binaries (if they don't have them yet)
chmod +x /ce/app/cosmosex
chmod +x /ce/update/flash_stm32
chmod +x /ce/update/flash_xilinx
chmod +x /ce/*.sh
chmod +x /ce/update/*.sh

#--------------------------
# check what chips we really need to flash

hans_curr=$( read_from_file /ce/update/hans.curent 0 )
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

#------------------------
# if should reboot after this (e.g. due to network settings reset), do it now
if [ -f /tmp/REBOOT_AFTER_UPDATE ]; then
    rm -f /tmp/REBOOT_AFTER_UPDATE          # delete file so we won't do it again next time
    reboot now
fi

echo " "
echo "Update done, you may start the /ce/ce_start.sh now!";
echo " "


