#!/bin/sh

cd /tmp
rm -f time.php time.sh

wget http://joo.kie.sk/cosmosex/time.php
mv time.php time.sh
chmod +x time.sh
./time.sh

