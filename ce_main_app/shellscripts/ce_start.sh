#!/bin/sh

ce_is_running() {
    # count instances of cosmosex app
    cnt=$( ps -A | grep cosmosex | wc -l )

    # more than zero? return true
    if [ "$cnt" -gt "0" ]; then
        return 1
    fi

    # count instances of cesuper.sh script
    cnt=$( ps -A | grep cesuper.sh | wc -l )

    # more than zero? return true
    if [ "$cnt" -gt "0" ]; then
        return 1
    fi

    # nothing is running, return false
    return 0
}

# kill previously running script and app
/ce/ce_stop.sh $1 $2

# remove doupdate script
rm -f /ce/update/doupdate.sh

# check if some chips need to be flashed, possibly force flash, and do the flashing (e.g. after new SD card being written)
/ce/update/check_and_flash_chips.sh

# if should start using systemctl, do it
if [ "$1" != "nosystemctl" ]; then
    # try to find out if cosmosex.service is installed
    sysctl=$( systemctl status cosmosex )
    sc_notfound=$( echo $sysctl | grep 'Loaded: not-found' | wc -l )

    # if the cosmosex.service is installed (the not-found string was not found)
    if [ "$sc_notfound" -eq "0" ]; then
        # try to start it using systemctl
        systemctl start cosmosex
    fi
fi

# check if ce is running
ce_is_running

# if CE is running, quit right away
if [ "$?" -eq "1" ]; then
    echo "cosmosex is running, exiting start script..."
    exit
fi

#------------------------
# run the cesuper script on background
echo "Starting CosmosEx supervisor script and CosmosEx app"
/ce/cesuper.sh > /dev/null 2>&1 &

