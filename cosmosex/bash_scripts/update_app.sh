#!/bin/sh

# Do app upgrade
# requires: existing /tmp/app.zip

echo "----------------------------------"
echo ">>> Updating Main App - START"
rm -rf /ce/app
mkdir /ce/app
cd /ce/app
mv /tmp/app.zip /ce/app
unzip /ce/app/app.zip
chmod 755 /ce/app/cosmosex
rm -f /ce/app/app.zip
sync
echo ">>> Updating Main App - END"
echo "----------------------------------"

