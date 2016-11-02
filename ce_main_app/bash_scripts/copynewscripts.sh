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

#compareAndCopy  local file                      new file
compareAndCopy   "/ce/ceboot.sh"                 "/tmp/newscripts/ceboot.sh"
compareAndCopy   "/ce/cesuper.sh"                "/tmp/newscripts/cesuper.sh"
compareAndCopy   "/ce/wifisuper.sh"              "/tmp/newscripts/wifisuper.sh"
compareAndCopy   "/ce/ce_conf.sh"                "/tmp/newscripts/ce_conf.sh"
compareAndCopy   "/ce/ce_start.sh"               "/tmp/newscripts/ce_start.sh"
compareAndCopy   "/ce/ce_stop.sh"                "/tmp/newscripts/ce_stop.sh"
compareAndCopy   "/ce/ce_firstfw.sh"             "/tmp/newscripts/ce_firstfw.sh"
compareAndCopy   "/ce/ce_update.sh"              "/tmp/newscripts/ce_update.sh"
compareAndCopy   "/ce/update/test_xc9536xl.xsvf" "/tmp/newscripts/test_xc9536xl.xsvf"
compareAndCopy   "/ce/update/test_xc9572xl.xsvf" "/tmp/newscripts/test_xc9572xl.xsvf"
compareAndCopy   "/ce/update/update_app.sh"      "/tmp/newscripts/update_app.sh"
compareAndCopy   "/ce/update/update_franz.sh"    "/tmp/newscripts/update_franz.sh"
compareAndCopy   "/ce/update/update_hans.sh"     "/tmp/newscripts/update_hans.sh"
compareAndCopy   "/ce/update/update_xilinx.sh"   "/tmp/newscripts/update_xilinx.sh"
compareAndCopy   "/etc/init.d/cosmosex"          "/tmp/newscripts/initd_cosmosex"

# todo
# check distro, if raspbian
# check if link exists:
# link doesn't exist - create it
# link does exist - check where it goes, and and replace it if needed

#readlink -f /etc/rc6.d/K01cosmosex
#ln -s /etc/init.d/cosmosex /etc/rcS.d/S09cosmosex
#ln -s /etc/init.d/cosmosex /etc/rc6.d/K01cosmosex

echo "Doing sync..."
sync
echo "All files should be up to date now."
