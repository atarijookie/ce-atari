#!/bin/bash

# set that an error will stop this whole script
set -e

# delete and create dir for the cosmosex
echo "Creating CosmosEx home dir - /ce"
sudo rm -rf /ce
sudo mkdir /ce
sudo chmod 777 /ce
mkdir /ce/update
cd /ce/update

# download update list and flash tools
echo "Downloading flash tools and update list"
wget http://joo.kie.sk/cosmosex/first/flash_stm32 2> wget.err
wget http://joo.kie.sk/cosmosex/first/flash_xilinx 2> wget.err
wget http://joo.kie.sk/cosmosex/update/updatelist.csv 2> wget.err
rm -f wget.err

# change permissions to be able to run this
chmod 755 flash_stm32
chmod 755 flash_xilinx

# get only column with urls from csv and wget it here
echo "Downloading update / install components"
sed 's/[^,]*,[^,]*,\([^,]*\).*/\1/' updatelist.csv | while read line; 
do
	wget $line 2> wget.err
done

rm -f wget.err

# now flash all the components to chips
echo "Writing firmware to flash of STM32 and Xilinx"
sudo ./flash_xilinx xilinx.xsvf
sudo ./flash_stm32 -x -w hans.hex /dev/ttyAMA0
sudo ./flash_stm32 -y -w franz.hex /dev/ttyAMA0

# create file containing just current xilinx firmware version
cat /ce/update/updatelist.csv | grep xilinx | sed -e 's/[^,]*,\([^,]*\).*/\1/' > /ce/update/xilinx_current.txt

# delete first installaction files
rm -f /ce/update/xilinx.xsvf
rm -f /ce/update/hans.hex
rm -f /ce/update/franz.hex

# create app dir and extract the app there
echo "Creating APP dir and extracting app there -- /ce/app"
mkdir /ce/app
cd /ce/app
mv /ce/update/app.zip /ce/app
unzip app.zip
chmod 755 /ce/app/cosmosex
rm -f app.zip

# download the CE supervisor / update script
echo "Downloading and setting supervisor and auto run script"
cd /ce
wget http://joo.kie.sk/cosmosex/first/cesuper.sh 2> wget.err
rm -f wget.err
chmod 755 cesuper.sh

# download and install the script for running the CE supervisor script
wget http://joo.kie.sk/cosmosex/first/rc.local 2> wget.err
rm -f wget.err
sudo rm -f /etc/rc.local
sudo mv /ce/rc.local /etc
sudo chmod 755 /etc/rc.local 

# done...
echo "Finished. You can reboot the device now and see if it did work."



