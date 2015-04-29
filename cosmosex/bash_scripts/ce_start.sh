#!/bin/sh

# See if cesuper.sh is running, and start it if it doesn't
cesuper_pid=$(ps | grep 'cesuper.sh' | grep -v 'grep' | awk '{print $1}' )

if [ -z $cesuper_pid]
then
	# remove leftover update script (if any)
	rm -f /ce/update/doupdate.sh

	echo "Starting cesuper.sh..."
	/ce/cesuper.sh &
else
	echo "cesuper.sh is running, not doing anything"
fi

