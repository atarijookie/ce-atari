#!/bin/sh

# kill previously running script and app
/ce/ce_stop.sh

# remove doupdate script
rm -f /ce/update/doupdate.sh

# run the cesuper script on background
echo "Starting CosmosEx supervisor script and CosmosEx app"
/ce/cesuper.sh > /dev/null 2> /dev/null &

