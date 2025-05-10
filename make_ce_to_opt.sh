#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

# make dirs for ce
mkdir -p /opt/ce/bin
mkdir -p /opt/ce/settings

cd libdospath
echo "building libdospath"
make -j4
cd -

cd ce_discovery
echo "building ce_discovery"
make -j4
cp ./ce_discovery.elf /opt/ce/bin
cd -

cd ce_core_hdd
echo "building ce_core_hdd"
make -j4
cp ./ce_hdd.elf /opt/ce/bin
cp -r ./configdrive /opt/ce/bin
cd -

# cd ce_core_fdd
# echo "building ce_core_fdd"
# make -j4
# cp ./ce_fdd.elf /opt/ce/bin
# cd -

# cd ce_core_ikbd
# echo "building ce_core_ikbd"
# make -j4
# cp ./ce_ikbd.elf /opt/ce/bin
# cd -

echo "copying webserver"
cp -r ./webserver /opt/ce/bin

echo "copying .env file"
cp ./.env /opt/ce/bin
ln -sf /opt/ce/bin/.env /opt/ce/bin/webserver/.env

echo "done"
