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
compareAndCopy   "/ce/ce_firstfw.sh"             "/ce/app/shellscripts/ce_firstfw.sh"
compareAndCopy   "/ce/ce_update.sh"              "/ce/app/shellscripts/ce_update.sh"
compareAndCopy   "/ce/update/test_xc9536xl.xsvf" "/ce/app/shellscripts/test_xc9536xl.xsvf"
compareAndCopy   "/ce/update/test_xc9572xl.xsvf" "/ce/app/shellscripts/test_xc9572xl.xsvf"
compareAndCopy   "/ce/update/update_app.sh"      "/ce/app/shellscripts/update_app.sh"
compareAndCopy   "/ce/update/update_franz.sh"    "/ce/app/shellscripts/update_franz.sh"
compareAndCopy   "/ce/update/update_hans.sh"     "/ce/app/shellscripts/update_hans.sh"
compareAndCopy   "/ce/update/update_xilinx.sh"   "/ce/app/shellscripts/update_xilinx.sh"

#---------------------------------------------------------------------------
# then copy the scripts which are different for Raspbian and Yocto
issue=$( cat /etc/issue | grep -o "Yocto" | wc -l ) 

# If at least once the Yocto was found, it's Yocto
if [ "$issue" -gt "0" ]; then
    # for yocto
    compareAndCopy   "/ce/wifisuper.sh"          "/ce/app/shellscripts/wifisuper.sh"         # update wifisuper.sh
else
    # for raspbian
	rm -f /ce/wifisuper.sh                                                              # remove wifisuper.sh
    compareAndCopy   "/ce/ceboot.sh"             "/ce/app/shellscripts/ceboot.sh"
    compareAndCopy   "/etc/init.d/cosmosex"      "/ce/app/shellscripts/initd_cosmosex"       # update init.d script
fi

echo "Doing sync..."
sync
echo "All files should be up to date now."
