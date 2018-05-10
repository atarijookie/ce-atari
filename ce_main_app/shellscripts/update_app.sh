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
    rm -f /ce/app/cosmosex_raspbian_jessie
    rm -f /ce/app/cosmosex_raspbian_stretch
else                            # on Raspbian - use raspbian, remove yocto
    rm -f /ce/app/cosmosex_yocto

    # if we have a single cosmosex_raspbian binary, just use it no matter what raspbian (jessie/stretch) it is
    if [ -f /ce/app/cosmosex_raspbian ]; then
        echo "Linux distro: Raspbian (universal)"
        echo "Using binary: cosmosex_raspbian"
        mv -f /ce/app/cosmosex_raspbian /ce/app/cosmosex
    else
        # if we don't have the universal cosmosex_raspbian binary, assume we have multiple raspbian binaries
        release=$( lsb_release -a 2>/dev/null )
        stretch=$( echo $release | grep -o stretch | wc -l)
        jessie=$( echo $release | grep -o jessie | wc -l)

        if [ "$stretch" -gt "0" ]; then
            echo "Linux distro: Raspbian Stretch"
            echo "Using binary: cosmosex_raspbian_stretch"
            mv -f /ce/app/cosmosex_raspbian_stretch /ce/app/cosmosex
        elif [ "$jessie" -gt "0" ]; then
            echo "Linux distro: Raspbian Jessie"
            echo "Using binary: cosmosex_raspbian_jessie"
            mv -f /ce/app/cosmosex_raspbian_jessie /ce/app/cosmosex
        else
            echo "Failed to match Raspbian release!"
            exit
        fi
    fi
fi
 
#----------------------

chmod +x /ce/app/cosmosex
rm -f /tmp/app.zip
sync
echo ">>> Updating Main App - END"
echo "----------------------------------"

