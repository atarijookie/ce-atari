#!/bin/sh

# first kill the cesuper.sh script so it won't restart cosmosex app
cesuper_pid=$(ps | grep 'cesuper.sh' | grep -v 'grep' | awk '{print $1}' )

if [ -z $cesuper_pid ]
then
	echo "cesuper.sh is not running, so not stopping it"
else
	echo "Stoping cesuper.sh ..."
	kill -9 $cesuper_pid
fi

# then kill the cosmosex app
cosmosex_pid=$(pidof cosmosex)

if [ -z $cosmosex_pid ]
then
	echo "cosmosex   is not running, so not stopping it"
else
	echo "Stopping cosmosex..."
	kill -9 $cosmosex_pid
fi

