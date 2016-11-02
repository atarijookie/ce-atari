#!/bin/sh
# App upgrade

echo "----------------------------------"
echo ">>> Updating Main App - START"

if [ ! -f /tmp/app.zip ]; then              # if this file doesn't exist, try to extract it from ZIP package
    if [ -f /tmp/ce_update.zip ]; then      # got the ZIP package, unzip
        unzip -o /tmp/ce_update.zip -d /tmp
    else                                    # no ZIP package? damn!
        echo "/tmp/app.zip and /tmp/ce_update.zip don't exist, can't update!"
        exit
    fi
fi

rm -rf /ce/app
mkdir /ce/app
unzip -o /tmp/app.zip -d /ce/app

#----------------------
# The following section should rename the right cosmosex app (for the right linux distro) to be used

# count the lines of /etc/issue which contain word Yocto in it.
issue=$( cat /etc/issue | grep -o "Yocto" | wc -l )

# If at least once the Yocto was found, it's Yocto
if [ "$issue" -gt "0" ]; then   # on Yocto - use yocto, remove raspbian
    echo "Linux distro: Yocto"
    echo "Using binary: cosmosex_yocto"

    mv -f /ce/app/cosmosex_yocto /ce/app/cosmosex
    rm -f /ce/app/cosmosex_raspbian
else                            # on Raspbian - use raspbian, remove yocto
    echo "Linux distro: Raspbian"
    echo "Using binary: cosmosex_raspbian"

    mv -f /ce/app/cosmosex_raspbian /ce/app/cosmosex
    rm -f /ce/app/cosmosex_yocto
fi
 
#----------------------

chmod +x /ce/app/cosmosex
rm -f /tmp/app.zip
sync
echo ">>> Updating Main App - END"
echo "----------------------------------"

