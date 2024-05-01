#!/bin/sh

. /ce/services/.env       # source env variables

echo "linux shell" > ${DATA_DIR}/app2.desc
/ce/services/appviasock.elf ${DATA_DIR}/app2.sock term
