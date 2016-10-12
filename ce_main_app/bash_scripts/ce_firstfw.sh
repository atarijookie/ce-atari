#!/bin/sh

# first stop any cosmosex process (script or app)
if [ "$1" != "nokill" ]; then
    echo "Stopping CosmosEx app and supervisor script."
    /ce/ce_stop.sh
fi

echo ""
echo "Reverting to first firmware, this will take a while."
echo "DO NOT POWER OFF THE DEVICE!!!"
echo ""

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

# detect Xilinx HW vs FW mismatch
out=$( /ce/app/cosmosex hwinfo )
mm=$( echo "$out" | grep 'HWFWMM' )

# if MISMATCH detected, flash xilinx again -- without programmed Hans the HW reporting from app could be wrong, and thus this one is needed to fix the situation
if [ "$mm" = "HWFWMM: MISMATCH" ]; then
    /ce/update/update_xilinx.sh
fi

# update franz
/ce/update/update_franz.sh

sync
echo ""
echo "Revert to first firmware done, you may start the /ce/ce_start.sh now!"
echo ""



