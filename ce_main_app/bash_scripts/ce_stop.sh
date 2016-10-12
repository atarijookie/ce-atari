#!/bin/sh

# first kill the cesuper.sh script so it won't restart cosmosex app
echo "Terminating cesuper.sh"
killall -9 cesuper.sh > /dev/null 2> /dev/null

# then kill the cosmosex app
echo "Terminating cosmosex"
killall -9 cosmosex > /dev/null 2> /dev/null

