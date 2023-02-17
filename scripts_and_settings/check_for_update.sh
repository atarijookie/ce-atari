#!/bin/sh

echo " "
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

PATH=$PATH:/ce/update     # add our update folder to path so we can use relative paths when referencing our scripts
DISTRO=$( distro.sh )     # fetch distro name

# no DISTRO still? quit
if [ -z "$DISTRO" ]; then
  echo "Cannot continue the update without DISTRO. Terminating."
  exit 1
fi

echo "running on distro    : $DISTRO"

#----------------------------------------------
# look for update on mounted disks

DATA_DIR=$( getdotenv.sh DATA_DIR "/var/run/ce/" )
UPDATE_FILE="$DISTRO.zip"
echo "searching on path    : $DATA_DIR"
echo "searching for file   : $UPDATE_FILE"

# find first matching file on possibly mounted USB in our DATA dir
UPDATE_PATH_USB=$( timeout 10s find $DATA_DIR -maxdepth 4 -name "$UPDATE_FILE" -follow -print -quit 2> /dev/null )

if [ -f "$UPDATE_PATH_USB" ]; then    # if file was found on some USB drive
  echo "found update on USB  : $UPDATE_PATH_USB"
  touch /tmp/UPDATE_PENDING_YES
  echo "got update for you!" > /tmp/UPDATE_STATUS
  status=$( cat /tmp/UPDATE_STATUS )
  echo "update status        : $status"

  path_to_tmp_update="/tmp/$DISTRO.zip"             # path where to store the ZIP file - either downloaded or from USB
  yes | cp $UPDATE_PATH_USB $path_to_tmp_update     # copy update to expected place
  echo "copied to tmp        : $path_to_tmp_update"

  exit 0                              # got update, quit now
else                                  # update not found on USB, try to search online
  echo "found update on USB  : no"
fi

#----------------------------------------------
# check for update online
UPDATE_URL=$( getdotenv.sh UPDATE_URL "http://joo.kie.sk/cosmosex/update" )
CE_DIR=$( getdotenv.sh CE_DIR "/ce" )
SETTINGS_DIR=$( getdotenv.sh SETTINGS_DIR "/ce/settings" )
LOG_FILE=/tmp/check_for_update_wget.log

UPDATE_VERSION_NEW_URL="$UPDATE_URL/$DISTRO.version"            # url to file containing new version for this distro
UPDATE_VERSION_NEW_LOCAL="/tmp/$DISTRO.version"                 # path to file containing new version for this distro
echo "online version url   : $UPDATE_VERSION_NEW_URL"
echo "online version path  : $UPDATE_VERSION_NEW_LOCAL"
timeout 5s wget -O $UPDATE_VERSION_NEW_LOCAL $UPDATE_VERSION_NEW_URL -o $LOG_FILE > /dev/null 2>&1    # download version from url

UPDATE_VERSION_CURR_LOCAL="$SETTINGS_DIR/$DISTRO.current"       # path to file containing current version for this distro

curr=$( read_from_file $UPDATE_VERSION_CURR_LOCAL 0 )     # read current version or 0
new=$( read_from_file $UPDATE_VERSION_NEW_LOCAL 1 )       # read new version or 1

echo "online version value : $new"
echo "current version path : $UPDATE_VERSION_CURR_LOCAL"
echo "current version value: $curr"

if [ "$new" != "$curr" ]; then
  echo "got update for you!" > /tmp/UPDATE_STATUS
  touch /tmp/UPDATE_PENDING_YES
  echo "update pending       : YES"
else
  echo "update not needed" > /tmp/UPDATE_STATUS
  touch /tmp/UPDATE_PENDING_NO
  echo "update pending       : NO"
fi

# show the final status
status=$( cat /tmp/UPDATE_STATUS )
echo "update status        : $status"
