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

# check if ce is running
ce_is_running
ce_running=$?

# if CE not running, quit right away
if [ "$ce_running" -eq "0" ]; then
    echo "cosmosex NOT running"
else
    echo "cosmosex IS running"
fi

distro=$( /ce/whichdistro.sh )

if [ "$distro" = "stretch" ]; then
    # check status through systemctl
    sysctl=$( systemctl status cosmosex )
    sc_notfound=$( echo $sysctl | grep 'Loaded: not-found' | wc -l )
    sc_running=$( echo $sysctl | grep 'Active: active' | wc -l )
    sc_notrunning=$( echo $sysctl | grep 'Active: failed' | wc -l )


    # now show the systemd service status
    if [ "$sc_notfound" -eq "1" ]; then
        echo "cosmosex not installed as systemd service"
    else
        if [ "$sc_running" -eq "1" ]; then
            echo "cosmosex IS running as systemd service"
        fi

        if [ "$sc_notrunning" -eq "1" ]; then
            echo "cosmosex NOT running as systemd service"
        fi
    fi
fi
