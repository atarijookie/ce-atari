#!/bin/sh

# check if running as root
if [ $(id -u) != 0 ]; then
  echo "Please run this as root"
  exit
fi

LOG_DIR="/var/log/ce"
mkdir -p "$LOG_DIR"                         # create log dir if doesn't exist
timestamp=$( date "+%Y%m%d%H%M%S" )
LOG_FILE="$LOG_DIR/check_for_update.log.$timestamp"   # generate logfile name with timestamp, so multiple runs of update can be observed

{           # start of block for output redirect
echo " "

start=$( date )
echo "check_for_update.sh started at : $start"     # show start time

echo "Checking for update"
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

# try to read currently stored (used) hash from file
hash_curr=$( read_from_file /ce/update/hash.current 0 )

distro=$( /ce/whichdistro.sh )          # get our distro name
echo "We're running on distro: $distro"

# check for update on mounted media
echo " "
echo "checking on disks" > /tmp/UPDATE_STATUS

mount | egrep -o '(/mnt/\S*)' | while read mnt      # go through found mounts
do
    hash_path="$mnt/$distro.hash"                   # hash file path
    update_path="$mnt/$distro.zip"                  # update file path

    echo "Checking mount: $mnt"

    if [ -f $update_path ] && [ -f $hash_path ]; then           # go both files? 
        hash_new=$( read_from_file $hash_path 0 )

        if [ "$hash_curr" != "$hash_new" ]; then                # current and new hash don't match? do update
            echo "Found new version here: $update_path - will update"
            echo "got update for you!" > /tmp/UPDATE_STATUS     # set new status so user can see that we've found update
            echo "$update_path" > /tmp/UPDATE_FROM_USB          # store path to this file, which serves also as a flag
            touch /tmp/UPDATE_PENDING_YES                       # create this flag that we should update

            stop=$( date )
            echo "check_for_update.sh finished at: $stop"       # show stop time
            exit                                                # now we can stop
        else
            echo "update $update_path - we already have that version"
        fi
    else
        echo "update file '$update_path' or '$hash_path' does not exist, ignoring this mount"
    fi
done

echo " "
echo "checking using wget" > /tmp/UPDATE_STATUS
url_hash="http://joo.kie.sk/cosmosex/update/$distro.hash"

# get last online hash for this distro
hash_new=$( wget -T 15 -O- $url_hash 2> /tmp/wget.err )

if [ $? -eq 0 ]; then   # if hash download went fine
    if [ "$hash_curr" != "$hash_new" ]; then    # current and new hash don't match? do update
        echo "Found new version online, will update"
        echo "( $hash_curr != $hash_new )"

        echo "got update for you!" > /tmp/UPDATE_STATUS
        touch /tmp/UPDATE_PENDING_YES
    else                # hashes are the same? don't update
        echo "No new update online, won't update"

        echo "update not needed" > /tmp/UPDATE_STATUS
        touch /tmp/UPDATE_PENDING_NO
    fi
else                    # if hash download failed
    echo "Failed to check for update online:"
    cat /tmp/wget.err   # show the error from wget

    echo "update check failed" > /tmp/UPDATE_STATUS
    touch /tmp/UPDATE_PENDING_NO
fi

stop=$( date )
echo " "
echo "check_for_update.sh finished at: $stop"     # show stop time

# end of block for output redirect to console and log file
} 2>&1 | tee $LOG_FILE
