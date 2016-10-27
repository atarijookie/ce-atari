#!/bin/sh

# kill previously running supervisor script
killall -9 cesuper.sh > /dev/null 2> /dev/null

# remove doupdate script
rm -f /ce/update/doupdate.sh

# kill the preinit app, if if exists and is runnig
killall -9 ce_preinit > /dev/null 2> /dev/null

#now start update monitoring loop
/ce/cesuper.sh & 
