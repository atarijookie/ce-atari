#!/bin/sh

compareAndCopy()
{
    if [ ! -f "$2" ]; then      # new file doesn't exist? quit
        echo "Skipping file $1 - new file doesn't exist!"
        return
    fi

    if [ -f "$1" ]; then        # if got the old file, compare
        cmp -s $1 $2

        if [ $? -eq "0" ]; then
            echo "Skipping file $1 - files are the same"
            return
        fi
    fi

    # copy the new file
    echo "Copying file $1"
    rm -f $1        # delete local file
    cp $2 $1        # copy new file to local file
    chmod +x $1     # make new local file executable
}

echo "Will compare with old and copy new files..."
mkdir -p /ce/update

#---------------------------------------------------------------------------
# first copy all the scripts which are the same for both Raspbian and Yocto

#compareAndCopy  local file                      new file
compareAndCopy   "/ce/cesuper.sh"                "/ce/app/shellscripts/cesuper.sh"
compareAndCopy   "/ce/ce_conf.sh"                "/ce/app/shellscripts/ce_conf.sh"
compareAndCopy   "/ce/ce_start.sh"               "/ce/app/shellscripts/ce_start.sh"
compareAndCopy   "/ce/ce_stop.sh"                "/ce/app/shellscripts/ce_stop.sh"
compareAndCopy   "/ce/ce_running.sh"             "/ce/app/shellscripts/ce_running.sh"
compareAndCopy   "/ce/ce_firstfw.sh"             "/ce/app/shellscripts/ce_firstfw.sh"
compareAndCopy   "/ce/ce_update.sh"              "/ce/app/shellscripts/ce_update.sh"
compareAndCopy   "/ce/update/test_xc9536xl.xsvf" "/ce/app/shellscripts/test_xc9536xl.xsvf"
compareAndCopy   "/ce/update/test_xc9572xl.xsvf" "/ce/app/shellscripts/test_xc9572xl.xsvf"
compareAndCopy   "/ce/update/update_app.sh"      "/ce/app/shellscripts/update_app.sh"
compareAndCopy   "/ce/update/update_franz.sh"    "/ce/app/shellscripts/update_franz.sh"
compareAndCopy   "/ce/update/update_hans.sh"     "/ce/app/shellscripts/update_hans.sh"
compareAndCopy   "/ce/update/update_xilinx.sh"   "/ce/app/shellscripts/update_xilinx.sh"
compareAndCopy   "/ce/install_ffmpeg.sh"         "/ce/app/shellscripts/install_ffmpeg.sh"
compareAndCopy   "/ce/time.sh"                   "/ce/app/shellscripts/time.sh"

#---------------------------------------------------------------------------
# then copy the scripts which are different for Raspbian and Yocto
issue=$( cat /etc/issue | grep -o "Yocto" | wc -l ) 

# If at least once the Yocto was found, it's Yocto
if [ "$issue" -gt "0" ]; then
    # for yocto
    compareAndCopy   "/ce/wifisuper.sh"          "/ce/app/shellscripts/wifisuper.sh"        # update wifisuper.sh
else
    # for raspbian
    rm -f /ce/wifisuper.sh                                                                  # remove wifisuper.sh
    compareAndCopy   "/ce/ceboot.sh"                        "/ce/app/shellscripts/ceboot.sh"
    compareAndCopy   "/etc/init.d/cosmosex"                 "/ce/app/shellscripts/initd_cosmosex"   # update init.d script
    compareAndCopy   "/etc/systemd/system/cosmosex.service" "/ce/app/shellscripts/cosmosex.service" # update systemd (systemctl) script

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
fi

echo "Doing sync..."
sync
echo "All files should be up to date now."
