# remove ce dir (if it exists)
rm -rf /ce
mkdir /ce
cd ce

# get package from server
wget http://joo.kie.sk/cosmosex/update/cosmosex_full.zip

# unzip content, set permissions
unzip /ce/cosmosex_full.zip
chmod 777 /ce/app/cosmosex
chmod 777 /ce/update/stm32flash
chmod 777 /ce/update/flash_xilinx

# flash the chips
/ce/update/stm32flash -x -w /ce/update/hans.hex /dev/ttyAMA0
/ce/update/stm32flash -y -w /ce/update/franz.hex /dev/ttyAMA0
/ce/update/flash_xilinx /ce/update/xilinx.xsvf


