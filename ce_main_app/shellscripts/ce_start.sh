#!/bin/sh

ce_is_running() {
    # find out whether we're on Yocto or Raspbian
    issue=$( cat /etc/issue | grep -o "Yocto" | wc -l )

    # If at least once the Yocto was found, it's Yocto
    if [ "$issue" -gt "0" ]; then   # on Yocto
        # Count how many instances are running
        cnt=$( ps | grep cosmosex | grep -v grep | wc -l )
    else                            # on Raspbian
        # Count how many instances are running
        cnt=$( ps -A | grep cosmosex | wc -l )
    fi

    # CosmosEx app is not running?
    if [ "$cnt" -eq "0" ]; then
        return 0
    fi

    # CosmosEx app is running
    return 1
}

# kill previously running script and app
/ce/ce_stop.sh $1

# remove doupdate script
rm -f /ce/update/doupdate.sh

#------------------------
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
    echo "cosmosex is running, terminating start script..."
    exit
fi

#------------------------
# run the cesuper script on background
echo "Starting CosmosEx supervisor script and CosmosEx app"
/ce/cesuper.sh > /dev/null 2> /dev/null &

