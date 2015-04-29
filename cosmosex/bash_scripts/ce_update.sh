#!/bin/sh

# first stop any cosmosex process (script or app)
/ce/ce_stop.sh

echo -e "\nUpdating CosmosEx from internet, this will take a while\nDO NOT POWER OFF THE DEVICE!!!\n"

# download the update package
cd /tmp/
rm -f /tmp/*.zip /tmp/*.hex /tmp/*.csv /tmp/*.xsvf

echo "\n>>> Downloading the update from web..."
wget http://joo.kie.sk/cosmosex/update/ce_update.zip

if [ ! -f "/tmp/ce_update.zip" ]
then
	echo "File /tmp/ce_update.zip not found, did the download fail?"
	exit 0
fi

unzip -o /tmp/ce_update.zip -d /tmp

# update the app
/ce/update/update_app.sh

# update xilinx
/ce/update/update_xilinx.sh

# update hans
/ce/update/update_hans.sh

# update franz
/ce/update/update_franz.sh

rm -f /tmp/*.zip /tmp/*.hex /tmp/*.csv /tmp/*.xsvf
sync

echo -e "\nUpdate done, you may start the /ce/ce_start.sh now!\n";


