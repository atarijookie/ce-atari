#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

PATH=$PATH:/ce/update     # add our update folder to path so we can use relative paths when referencing our scripts

# get log path, generate current date and logfile with that date, redirect output to log and console
LOG_DIR=$( getdotenv.sh LOG_DIR "/var/log/ce" )
NOW=$( date +"%Y%m%d_%H%M%S" )
LOG_FILE="$LOG_DIR/update_check_$NOW.log"
{

# read these .env variables for usage further below
FILE_UPDATE_STATUS=$( getdotenv.sh FILE_UPDATE_STATUS "" )
FILE_UPDATE_PENDING_YES=$( getdotenv.sh FILE_UPDATE_PENDING_YES "" )
FILE_UPDATE_PENDING_NO=$( getdotenv.sh FILE_UPDATE_PENDING_NO "" )

rm -f "$FILE_UPDATE_STATUS" "$FILE_UPDATE_PENDING_YES" "$FILE_UPDATE_PENDING_NO"   # delete update pending answer files
echo "Checking for update..." | tee "$FILE_UPDATE_STATUS"                 # fill update check status

read_from_file()
{
  # $1 - path to file
  # $2 - default value if file not exists
  [ -f "$1" ] && cat $1 || echo $2
}

online_file_download()
{
  # $1 - url from which the file should be downloaded
  # $2 - local path where the file will be stored
  timeout 5s wget -O $2 $1 -o /tmp/wget.log > /dev/null 2>&1  # download from url
  [ $? -ne 0 ] && rm -f $2                # wget failed? delete output file, it's empty or bad
}

online_file_exists()
{
  # $1 - url which should be checked for existence
  timeout 5s wget -S --spider $1 -o /tmp/wget.log 2>&1
  cat /tmp/wget.log | grep 'HTTP/1.1 200 OK' > /dev/null 2>&1
  echo $?                 # result from grep
}

update_yes()
{
  echo "got update for you!" > "$FILE_UPDATE_STATUS"
  touch "$FILE_UPDATE_PENDING_YES"
  echo "update pending       : YES"
  echo "created file         : $FILE_UPDATE_PENDING_YES"
  echo -n "update status        : " ; cat "$FILE_UPDATE_STATUS"     # show the final status
}

update_no()
{
  # $1 - reason for no update. If not provided, will use default
  [ -z "$1" ] && STATUS="update not needed" || STATUS=$1
  echo $STATUS > "$FILE_UPDATE_STATUS"
  touch "$FILE_UPDATE_PENDING_NO"
  echo "update pending       : NO"
  echo "created file         : $FILE_UPDATE_PENDING_NO"
  echo -n "update status        : " ; cat "$FILE_UPDATE_STATUS"     # show the final status
}

DISTRO=$( distro.sh )     # fetch distro name
[ -z "$DISTRO" ] && echo "Cannot continue the update without DISTRO. Terminating." && exit 1    # no DISTRO still? quit
echo "running on distro    : $DISTRO"

#----------------------------------------------
# look for update on mounted disks

UPDATE_PATH_USB=$( check_for_update_usb.sh )
[ -f "$UPDATE_PATH_USB" ] && update_yes && exit 0         # if file was found on some USB drive

#----------------------------------------------
# check for update online
UPDATE_URL=$( getdotenv.sh UPDATE_URL "http://joo.kie.sk/cosmosex/update" )
CE_DIR=$( getdotenv.sh CE_DIR "/ce" )
CE_UPDATE_DIR=$( getdotenv.sh CE_UPDATE_DIR "/ce/update" )

UPDATE_VERSION_NEW_URL="$UPDATE_URL/$DISTRO.version"        # url to file containing new version for this distro
UPDATE_VERSION_NEW_LOCAL="/tmp/$DISTRO.version"             # path to file containing new version for this distro
UPDATE_VERSION_CURR_LOCAL="$CE_UPDATE_DIR/$DISTRO.current"   # path to file containing current version for this distro

echo "online version url   : $UPDATE_VERSION_NEW_URL"
echo "online version path  : $UPDATE_VERSION_NEW_LOCAL"

url_exists=$( online_file_exists $UPDATE_VERSION_NEW_URL )                    # check if url exists or not
[ "$url_exists" != "0" ] && update_no "update url not reachable" && exit 1    # if update url doesn't exist, no update

online_file_download $UPDATE_VERSION_NEW_URL $UPDATE_VERSION_NEW_LOCAL        # download the file from url

new=$( read_from_file $UPDATE_VERSION_NEW_LOCAL 1 )         # read new version or 1
curr=$( read_from_file $UPDATE_VERSION_CURR_LOCAL 0 )       # read current version or 0

echo "online version value : $new"
echo "current version path : $UPDATE_VERSION_CURR_LOCAL"
echo "current version value: $curr"

[ "$new" != "$curr" ] && update_yes || update_no            # got update if versions don't match, otherwise no update

} 2>&1 | tee $LOG_FILE      # end of redirect to file and console
