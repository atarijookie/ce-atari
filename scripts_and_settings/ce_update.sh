#!/bin/sh

echo " "
echo "Updating CosmosEx from internet, this will take a while."
echo "DO NOT POWER OFF THE DEVICE!!!"
echo " "

PATH=$PATH:/ce/update     # add our update folder to path so we can use relative paths when referencing our scripts
DISTRO=$( distro.sh )     # fetch distro name

# no DISTRO still? quit
if [ -z "$DISTRO" ]; then
  echo "Cannot continue the update without DISTRO. Terminating."
  exit 1
fi

UPDATE_URL=$( getdotenv.sh UPDATE_URL "http://joo.kie.sk/cosmosex/update" )
CE_DIR=$( getdotenv.sh CE_DIR "/ce" )
SETTINGS_DIR=$( getdotenv.sh SETTINGS_DIR "/ce/settings" )

url_to_zip_update="$UPDATE_URL/$DISTRO.zip"        # URL where to get the ZIP file
path_to_tmp_update="/tmp/$DISTRO.zip"             # path where to store the ZIP file - either downloaded or from USB

# back up current CE settings
echo "Backing up settings."
rm -rf /tmp/ce_settings
mkdir -p /tmp/ce_settings
cp -r $SETTINGS_DIR/* /tmp/ce_settings

# First check if we have the archive already downloaded - e.g. for update from USB, or from previous download.
# Then check if the archive is valid and delete it if it's not.
if [ -f $path_to_tmp_update ]; then               # update file already exists?
  echo "Update archive found: $path_to_tmp_update"
  unzip -t $path_to_tmp_update > /dev/null 2>&1   # test archive

  if [ $? -ne 0 ]; then                           # if archive test failed, delete archive
    echo "Update archive is corrupted, deleting."
    rm -f $path_to_tmp_update
  fi
fi

# if after previous check we don't have the archive, we need to download it now
if [ ! -f $path_to_tmp_update ]; then             # update file doesn't exists (now)?
  echo "Will now download update archive from $url_to_zip_update to $path_to_tmp_update"
  wget -O $path_to_tmp_update $url_to_zip_update  # download to some local temp dir
fi

# stop CE services
ce stop

unzip -o $path_to_tmp_update -d $CE_DIR           # unzip update into /ce directory, overwrite without prompting

if [ $? -ne 0 ]; then                             # unzip failed?
  echo "Failed to unzip $path_to_tmp_update , cannot continue. Terminating."
  ce start                                        # start CE services
  exit 1
end

#--------------------------
# add execute permissions to scripts and binaries
echo "Adding execute permission to scripts and binaries."
find /ce/ -type f -name "*.sh" -exec chmod +x {} \;
find /ce/ -type f -name "*.elf" -exec chmod +x {} \;

#--------------------------
# check what chips we really need to flash and flash those
check_and_flash_chips.sh

#--------------------------
# on  update systemctl service
cp "/ce/cosmosex.service" "/etc/systemd/system/cosmosex.service"

# create a symlink to CE script so user can use 'ce' command
ln -fs /ce/services/ce.sh /usr/local/bin/ce

echo "Restoring settings."
cp -r /tmp/ce_settings* $SETTINGS_DIR/

#------------------------
# now add / change the core_freq param in the boot config to avoid SPI clock issues
coreFreqCountAny=$( cat /boot/config.txt | grep core_freq | wc -l )
coreFreqCountCorrect=$( cat /boot/config.txt | grep 'core_freq=250' | wc -l )

addCoreFreq=0           # don't add it yet

# A) no core_freq? Add it
if [ "$coreFreqCountAny" -eq "0" ]; then
    echo "No core_freq in /boot/config.txt, adding it"
    addCoreFreq=1
fi

# B) more than one core_freq? Remove them, then add one
if [ "$coreFreqCountAny" -gt "1" ]; then
    echo "Too many core_freq in /boot/config.txt, fixing it"
    addCoreFreq=1
fi

# C) There is some core_freq, but it's not correct? Remove it, then add correct one
if [ "$coreFreqCountAny" -gt "0" ] && [ "$coreFreqCountCorrect" -eq "0" ]; then
    echo "core_freq in /boot/config.txt is incorrect, fixing it"
    addCoreFreq=1
fi

# if we need to add the core_freq
if [ "$addCoreFreq" -gt "0" ]; then
    mv /boot/config.txt /boot/config.old                            # back up old
    cat /boot/config.old | grep -v 'core_freq' > /boot/config.txt   # create new without any core_freq directive

    # now append the correct core_config on the end of file
    echo "core_freq=250" >> /boot/config.txt
else
    # we don't need to do anything for case D), where there is one core_freq there and it's correct
    echo "core_freq is ok in /boot/config.txt"
fi

#------------------------
# disable ctrl-alt-del causing restart
ln -fs /dev/null /lib/systemd/system/ctrl-alt-del.target

# disable auto-login
ln -fs /lib/systemd/system/getty@.service /etc/systemd/system/getty.target.wants/getty@tty1.service

sync

#------------------------
# if should reboot after this (e.g. due to network settings reset), do it now
if [ -f /tmp/REBOOT_AFTER_UPDATE ]; then
    rm -f /tmp/REBOOT_AFTER_UPDATE          # delete file so we won't do it again next time
    reboot now
fi

# start the CosmosEx servivce
ce start

# final message
echo " "
echo "Update done, the software should start up in few seconds.";
echo " "
