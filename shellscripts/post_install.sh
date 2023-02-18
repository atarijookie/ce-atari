#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

echo "Post install script..."

# do the following section only when the boot config file is present (e.g. we're on Raspberry Pi)
if [ -f /boot/config.txt ]; then
  # add / change the core_freq param in the boot config to avoid SPI clock issues
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
fi

#------------------------
# disable ctrl-alt-del causing restart
ln -fs /dev/null /lib/systemd/system/ctrl-alt-del.target

# disable auto-login
ln -fs /lib/systemd/system/getty@.service /etc/systemd/system/getty.target.wants/getty@tty1.service
