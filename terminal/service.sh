#!/bin/sh

echo "linux shell" > /var/run/ce/app2.desc
/ce/services/appviasock.elf /var/run/ce/app2.sock term
