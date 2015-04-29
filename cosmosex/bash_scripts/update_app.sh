#!/bin/sh
# App upgrade

echo "----------------------------------"
echo ">>> Updating Main App - START"

if [ ! -f /tmp/app.zip ]; then              # if this file doesn't exist, try to extract it from ZIP package
    if [ -f /tmp/ce_update.zip ]; then      # got the ZIP package, unzip
        unzip -o /tmp/ce_update.zip -d /tmp
    else                                    # no ZIP package? damn!
        echo "/tmp/app.zip and /tmp/ce_update.zip don't exist, can't update!"
        exit
    fi
fi

rm -rf /ce/app
mkdir /ce/app
unzip -o /tmp/app.zip -d /ce/app
chmod 755 /ce/app/cosmosex
rm -f /tmp/app.zip
sync
echo ">>> Updating Main App - END"
echo "----------------------------------"

