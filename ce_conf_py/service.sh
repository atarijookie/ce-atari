#!/bin/sh

echo "CosmosEx config" > /var/run/ce/app0.desc
/ce/services/appviasock.elf /var/run/ce/app0.sock /ce/services/config/ce_conf.sh
