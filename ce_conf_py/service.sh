#!/bin/sh

. /ce/services/.env       # source env variables

echo "CosmosEx config" > ${DATA_DIR}/app0.desc
/ce/services/appviasock.elf ${DATA_DIR}/app0.sock /ce/services/config/ce_conf.sh
