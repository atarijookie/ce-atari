#!/bin/sh

compareAndCopy()
{
    if [ ! -f "$2" ]; then      # new file doesn't exist? quit
        echo "New file missing, skipping file: $1"
        return
    fi

    if [ -f "$1" ]; then        # if got the old file, compare
        cmp -s $1 $2

        if [ $? -eq "0" ]; then
            #echo "Files match, skipping file     : $1"
            return
        fi
    fi

    # copy the new file
    echo "Copying file                   : $1"
    rm -f $1        # delete local file
    cp $2 $1        # copy new file to local file
    chmod +x $1     # make new local file executable
}

mkdir -p /ce/update

#---------------------------------------------------------------------------
# copy files which are needed for change from old update system to new update system

#compareAndCopy  local file                         new file
compareAndCopy   "/ce/ce_start.sh"                  "/ce/app/shellscripts/ce_start.sh"
compareAndCopy   "/ce/ce_stop.sh"                   "/ce/app/shellscripts/ce_stop.sh"
compareAndCopy   "/ce/update/check_for_update.sh"   "/ce/app/shellscripts/check_for_update.sh"
compareAndCopy   "/ce/whichdistro.sh"               "/ce/app/shellscripts/whichdistro.sh"
compareAndCopy   "/ce/ce_update.sh"                 "/ce/app/shellscripts/ce_update.sh"
compareAndCopy   "/ce/update/update_franz.sh"       "/ce/app/shellscripts/update_franz.sh"
compareAndCopy   "/ce/update/update_hans.sh"        "/ce/app/shellscripts/update_hans.sh"
compareAndCopy   "/ce/update/update_xilinx.sh"      "/ce/app/shellscripts/update_xilinx.sh"

echo "Doing sync..."
sync
echo "All files should be up to date now."
