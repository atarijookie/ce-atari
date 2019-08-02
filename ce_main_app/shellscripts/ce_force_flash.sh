#!/bin/sh

echo "Forcing writing of flash of all chips!"
touch /tmp/FW_FLASH_ALL     # create this file, so ce_update.sh will do flashing in any case
/ce/ce_update.sh            # run the update
