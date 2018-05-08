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

# check if ce is running
ce_is_running

# if CE not running, quit right away
if [ $? -eq 0 ]; then
    echo "cosmosex NOT running"
else
    echo "cosmosex IS running"
fi
