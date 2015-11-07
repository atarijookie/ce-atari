#!/bin/sh

# kill previously running script and app
killall -9 cesuper.sh > /dev/null 2> /dev/null
killall -9 cosmosex > /dev/null 2> /dev/null

# remove doupdate script
rm -f /ce/update/doupdate.sh

# run the cesuper script on background
echo "Starting CosmosEx supervisor script and CosmosEx app"
/ce/cesuper.sh &

