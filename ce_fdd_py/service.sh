#!/bin/sh

echo "Floppy Tool" > /var/run/ce/app1.desc
/ce/services/appviasock.elf /var/run/ce/app1.sock /ce/services/floppy/ce_fdd.sh
