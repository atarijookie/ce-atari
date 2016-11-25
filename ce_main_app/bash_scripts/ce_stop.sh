#!/bin/sh

# first kill the cesuper.sh script so it won't restart cosmosex app
echo "Terminating cesuper.sh"
killall -9 cesuper.sh > /dev/null 2> /dev/null

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
