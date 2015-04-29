#!/bin/sh

# first stop any cosmosex process (script or app)
/ce/ce_stop.sh

echo -e "\nReverting to first firmware, this will take a while.\nDO NOT POWER OFF THE DEVICE!!!\n\n"

# remove all the settings - if they would be cause of some issue, this could help
rm -f /ce/settings/*

# copy first FW to tmp - that's where the update scripts expect things to be
cp /ce/firstfw/* /tmp/

# update the app
/ce/update/update_app.sh

# update xilinx
/ce/update/update_xilinx.sh

# update hans
/ce/update/update_hans.sh

# update franz
/ce/update/update_franz.sh

sync
echo -e "\n\nRevert to first firmware done, you may start the /ce/ce_start.sh now!";


