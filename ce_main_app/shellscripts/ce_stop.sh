#!/bin/sh

ce_is_running() {
    # Count how many instances are running
    cnt=$( ps -A | grep cosmosex | wc -l )

    # CosmosEx app is not running?
    if [ "$cnt" -eq "0" ]; then
        return 0
    fi

    # CosmosEx app is running
    return 1
}

# kill the cesuper.sh script so it won't restart cosmosex app
# don't kill cesuper.sh if requested, e.g. when called ce_main_app starts update, which runs ce_update.sh, which runs within cesuper
# (then it only stops possibly running cosmosex app, but not the other scripts which might have cesuper.sh as parent in shell)
if [ "$2" != "dontkillcesuper" ]; then
    echo "Terminating cesuper.sh"
    killall -9 cesuper.sh > /dev/null 2>&1
fi

# check if ce is running
ce_is_running

# if CE not running, quit right away
if [ "$?" -eq "0" ]; then
    echo "Terminating cosmosex not needed, no instance is running"
    exit
fi

#------------------------
# if should terminate (also) using systemctl, do it
if [ "$1" != "nosystemctl" ]; then
    # try to find out if cosmosex.service is installed
    sysctl=$( systemctl status cosmosex )
    sc_notfound=$( echo $sysctl | grep 'Loaded: not-found' | wc -l )

    # if the cosmosex.service is installed (the not-found string was not found)
    if [ "$sc_notfound" -eq "0" ]; then
        # try to stop it using systemctl
        systemctl stop cosmosex
    fi
fi

#------------------------
# send SIGINT to allow terminate the app nicely
echo "Terminating cosmosex - SIGINT"
killall -2 cosmosex > /dev/null 2> /dev/null

# wait a while
echo "Waiting for cosmosex to terminate nicely..."

# as Yocto and Raspbian shells behave differently, there could be a loop here which would work on both, but...
ce_is_running
if [ "$?" -eq "0" ]; then
    echo "cosmosex terminated"
    exit
fi
sleep 1

ce_is_running
if [ "$?" -eq "0" ]; then
    echo "cosmosex terminated"
    exit
fi
sleep 1

ce_is_running
if [ "$?" -eq "0" ]; then
    echo "cosmosex terminated"
    exit
fi
sleep 1

# send SIGKILL to terminate the app if it didn't stop nicely
echo "Terminating cosmosex - SIGKILL"
killall -9 cosmosex > /dev/null 2> /dev/null
