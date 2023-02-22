#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

# get log path, generate current date and logfile with that date, redirect output to log and console
PATH=$PATH:/ce/update       # add our update folder to path so we can use relative paths when referencing our scripts
LOG_DIR=$( getdotenv.sh LOG_DIR "/var/log/ce" )
NOW=$( date +"%Y%m%d_%H%M%S" )
LOG_FILE="$LOG_DIR/update_$NOW.log"
{
echo " "
echo "Updating CosmosEx from internet, this will take a while."
echo "DO NOT POWER OFF THE DEVICE!!!"
echo " "

online_file_download()
{
  # $1 - url from which the file should be downloaded
  # $2 - local path where the file will be stored
  rm -f /tmp/wget.log
  timeout 120s wget -O $2 $1 -o /tmp/wget.log > /dev/null 2>&1  # download from url
  [ $? -ne 0 ] && rm -f $2                # wget failed? delete output file, it's empty or bad
}

online_file_exists()
{
  # $1 - url which should be checked for existence
  rm -f /tmp/wget.log
  timeout 5s wget -S --spider $1 -o /tmp/wget.log 2>&1
  cat /tmp/wget.log | grep 'HTTP/1.1 200 OK' > /dev/null 2>&1
  echo $?                 # result from grep
}

settings_backup()
{
  # back up current CE settings
  SETTINGS_DIR=$( getdotenv.sh SETTINGS_DIR "/ce/settings" )
  BACKUP_DIR=/tmp/ce_settings
  echo "backup settings      : $SETTINGS_DIR  =>  $BACKUP_DIR"

  rm -rf $BACKUP_DIR
  mkdir -p $BACKUP_DIR
  cp -r $SETTINGS_DIR/* $BACKUP_DIR
}

settings_restore()
{
  SETTINGS_DIR=$( getdotenv.sh SETTINGS_DIR "/ce/settings" )
  BACKUP_DIR=/tmp/ce_settings
  echo "restore settings     : $BACKUP_DIR  =>  $SETTINGS_DIR"

  cp -r $BACKUP_DIR/* $SETTINGS_DIR
}

DISTRO=$( distro.sh )                       # fetch distro name
[ -z "$DISTRO" ] && echo "Cannot continue the update without DISTRO. Terminating." && exit 1    # no DISTRO still? quit
echo "running on distro    : $DISTRO"
UPDATE_LOCAL="/tmp/$DISTRO.zip"             # path where to store the ZIP file - either downloaded or from USB

UPDATE_PATH_USB=$( check_for_update_usb.sh )    # look for update on mounted disks
if [ -f "$UPDATE_PATH_USB" ]; then                # if file was found on some USB drive
  echo "update source        : locally mounted disk"
  echo "moving update        : $UPDATE_PATH_USB  =>  $UPDATE_LOCAL"
  mv "$UPDATE_PATH_USB" "$UPDATE_LOCAL"
else                                              # no file from USB, try to fetch it online
  UPDATE_BASE_URL=$( getdotenv.sh UPDATE_URL "http://joo.kie.sk/cosmosex/update" )
  UPDATE_URL="$UPDATE_BASE_URL/$DISTRO.zip"       # URL where to get the ZIP file

  echo "update source        : internet"
  echo "downloading update   : $UPDATE_URL  =>  $UPDATE_LOCAL"

  url_exists=$( online_file_exists $UPDATE_URL )  # check if url exists or not
  [ "$url_exists" != "0" ] && echo "update fail          : url $UPDATE_URL not reachable!" && exit 1    # if update url doesn't exist, no update

  online_file_download $UPDATE_URL $UPDATE_LOCAL  # download the file from url
fi

# Check if the archive is valid and delete it if it's not and quit.
if [ -f $UPDATE_LOCAL ]; then               # update file already exists?
  echo "update archive found : $UPDATE_LOCAL"
  unzip -t $UPDATE_LOCAL > /dev/null 2>&1   # test archive

  # fail here if archive is bad
  [ $? -ne 0 ] && echo "update fail          : archive $UPDATE_LOCAL corrupted!" && rm -f $UPDATE_LOCAL && exit 1
fi

# do the actual update now
settings_backup                         # backup CE settings

# unzip archive
CE_DIR=$( getdotenv.sh CE_DIR "/ce" )
unzip -o $UPDATE_LOCAL -d $CE_DIR       # unzip update into /ce directory, overwrite without prompting
[ $? -ne 0 ] && echo "update fail          : unzip $UPDATE_LOCAL failed!." && rm -f $UPDATE_LOCAL && exit 1  # exit if failed to unzip

# delete archive
echo "delete after unzip   : $UPDATE_LOCAL"
rm -f $UPDATE_LOCAL

# add permissions
echo "adding X permissions : *.sh, *.elf"
find /ce/ -type f \( -name "*.sh" -o -name "*.elf" \) -exec chmod +x {} \;

settings_restore                        # restore settings
flash_chips.sh                          # check what chips we really need to flash and flash those

#--------------------------
ln -fs /ce/services/ce.sh /usr/local/bin/ce       # create a symlink to CE script so user can use 'ce' command

# run the post install script
post_install.sh
sync

# if should reboot after this (e.g. due to network settings reset), do it now
[ -f /tmp/REBOOT_AFTER_UPDATE ] && rm -f /tmp/REBOOT_AFTER_UPDATE && reboot now && exit 0

# starting of CosmosEx servivce will happen updater service
echo " "
echo "Update done, the software should start up in few seconds.";
echo " "

} 2>&1 | tee $LOG_FILE    # end of redirect to file and console
