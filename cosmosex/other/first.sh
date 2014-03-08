# todo:
# download and chmod cesuper.sh
# set up that cesuper.sh will be started after device start (/etc/rc.d)
# create xilinx version file
# fix flash_stm32 for franz
# delete first install files
# do device restart


# delete and create dir for the cosmosex
sudo rm -rf /ce
sudo mkdir /ce
sudo chmod 777 /ce
mkdir /ce/update
cd /ce/update

# download update list and flash tools
wget http://joo.kie.sk/cosmosex/update/updatelist.csv
wget http://joo.kie.sk/cosmosex/update/flash_stm32
wget http://joo.kie.sk/cosmosex/update/flash_xilinx

# change permissions to be able to run this
chmod 755 flash_stm32
chmod 755 flash_xilinx

# get only column with urls from csv and wget it here
sed 's/[^,]*,[^,]*,\([^,]*\).*/\1/' updatelist.csv | while read line; 
do
	wget $line
done

# now flash all the components to chips
sudo ./flash_xilinx xilinx.xsvf
sudo ./flash_stm32 -x -w hans.hex /dev/ttyAMA0
sudo ./flash_stm32 -y -w franz.hex /dev/ttyAMA0

# unzip the app
cd /ce
mv /ce/update/app.zip /ce
unzip app.zip
chmod 777 /ce/app/cosmosex

