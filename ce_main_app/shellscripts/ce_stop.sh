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

# first kill the cesuper.sh script so it won't restart cosmosex app
echo "Terminating cesuper.sh"
killall -9 cesuper.sh > /dev/null 2>&1

# check if ce is running
ce_is_running

# if CE not running, quit right away
if [ "$?" -eq "0" ]; then
    echo "Terminating cosmosex not needed, no instance is running"
    exit
fi

#------------------------
# check for distro, run systemctl only on stretch
distro=$( /ce/whichdistro.sh ) 

if [ "$distro" = "raspbian_stretch" ]; then
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
fi

#------------------------

# send SIGINT to allow terminate the app nicely
echo "Terminating cosmosex - SIGINT"
killall -2 cosmosex                  > /dev/null 2> /dev/null
killall -2 cosmosex_raspbian         > /dev/null 2> /dev/null
killall -2 cosmosex_raspbian_stretch > /dev/null 2> /dev/null
killall -2 cosmosex_raspbian_jessie  > /dev/null 2> /dev/null
killall -2 cosmosex_yocto            > /dev/null 2> /dev/null

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
killall -9 cosmosex                  > /dev/null 2> /dev/null
killall -9 cosmosex_raspbian         > /dev/null 2> /dev/null
killall -9 cosmosex_raspbian_stretch > /dev/null 2> /dev/null
killall -9 cosmosex_raspbian_jessie  > /dev/null 2> /dev/null
killall -9 cosmosex_yocto            > /dev/null 2> /dev/null
