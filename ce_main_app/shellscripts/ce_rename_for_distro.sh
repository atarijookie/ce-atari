#!/bin/sh

# Store this script in: ce_update.zip/app.zip/cosmosex

# This script should be placed in the CE update package and named 'cosmosex' - just like the original cosmosex app.
# The update package should also contain 'cosmosex_yocto' and 'cosmosex_raspbian' binaries, which are cosmosex app compiled for yocto and raspbian linux distro.
# When the old cosmosex app (with the old /ce/update/update_app.sh) is updated, this will result in having /ce/app/ dir filled with
# this renaming script and both yocto and raspbian binaries, this renaming script is then executed by 'cesuper.sh', it chooses and renames the correct binary
# to become the new real 'cosmosex' binary, it terminates, and then the new binary is started by the 'cesuper.sh', resulting with having the right binary based on your linux distro.

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

# make cosmosex executable, if it isn't already
chmod +x /ce/app/cosmosex
sync
