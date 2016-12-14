#!/bin/sh

# first kill the cesuper.sh script so it won't restart cosmosex app
echo "Terminating cesuper.sh"
killall -9 cesuper.sh > /dev/null 2> /dev/null

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

# CosmosEx app is not running at all? quit
if [ "$cnt" -eq "0" ]; then
    echo "Terminating cosmosex not needed, no instance is running"
    exit
fi

# send SIGINT to allow terminate the app nicely
echo "Terminating cosmosex - SIGINT"
killall -2 cosmosex          > /dev/null 2> /dev/null
killall -2 cosmosex_raspbian > /dev/null 2> /dev/null
killall -2 cosmosex_yocto    > /dev/null 2> /dev/null

# wait a while
echo "Waiting 3 seconds..."
sleep 3

# send SIGKILL to terminate the app if it didn't stop nicely
echo "Terminating cosmosex - SIGKILL"
killall -9 cosmosex          > /dev/null 2> /dev/null
killall -9 cosmosex_raspbian > /dev/null 2> /dev/null
killall -9 cosmosex_yocto    > /dev/null 2> /dev/null
