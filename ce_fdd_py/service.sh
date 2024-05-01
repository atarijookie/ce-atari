#!/bin/sh

. /ce/services/.env       # source env variables

echo "Floppy Tool" > ${DATA_DIR}/app1.desc
/ce/services/appviasock.elf ${DATA_DIR}/app1.sock /ce/services/floppy/ce_fdd.sh
