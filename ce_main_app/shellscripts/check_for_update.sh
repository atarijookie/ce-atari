#!/bin/sh

echo " "

start=$( date )
echo "check_for_update.sh started at : $start"     # show start time

echo "Checking for update..."

echo "checking for update" > /tmp/UPDATE_STATUS

# delete both update_pending answer files
rm -f /tmp/UPDATE_PENDING_YES
rm -f /tmp/UPDATE_PENDING_NO

read_from_file()
{
    if [ -f $1 ]; then      # does the file exist? read value from file
        val=$( cat $1 )
    else                    # file doesn't exist? use default value
        val=$2
    fi

    echo $val               # display the value
}

distro=$( /ce/whichdistro.sh )
url_hash="http://joo.kie.sk/cosmosex/update/$distro.hash"

echo "We're running on distro: $distro"
echo "checking using wget" > /tmp/UPDATE_STATUS

# get last online hash for this distro
hash_new=$( wget -O- $url_hash 2> /dev/null )

# try to read currently stored (used) hash from file
hash_curr=$( read_from_file /ce/update/hash.current 0 )

if [ "$hash_curr" != "$hash_new" ]; then    # current and new hash don't match? do update
    echo "Found new version online, will update"
    echo "( $hash_curr != $hash_new )"

    echo "got update for you!" > /tmp/UPDATE_STATUS
    touch /tmp/UPDATE_PENDING_YES
else        # hashes are the same? don't update
    echo "No new update online, won't update"

    echo "update not needed" > /tmp/UPDATE_STATUS
    touch /tmp/UPDATE_PENDING_NO
fi

stop=$( date )
echo "check_for_update.sh finished at: $stop"     # show stop time
