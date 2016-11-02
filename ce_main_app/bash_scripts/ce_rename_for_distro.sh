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
else                            # on Raspbian - use raspbian, remove yocto
    echo "Linux distro: Raspbian"
    echo "Using binary: cosmosex_raspbian"

    mv -f /ce/app/cosmosex_raspbian /ce/app/cosmosex
    rm -f /ce/app/cosmosex_yocto
fi

# make cosmosex executable, if it isn't already
chmod +x /ce/app/cosmosex
sync
