#!/bin/sh

# Look for update on mounted disks. Will return valid path to update archive if found.
# Use this script when:
# - just checking for update existence on mounted disks
# - really updating this device and looking for the location of the update file before unpacking.

# Use this echoerr function to show messages to user, they will go to stderr, and use usual echo just
# to output the value which will be used as result from this script for the calling script.
echoerr() { echo "$@" 1>&2; }

PATH=$PATH:/ce/update     # add our update folder to path so we can use relative paths when referencing our scripts
DISTRO=$( distro.sh )     # fetch distro name

# no DISTRO still? quit
[ -z "$DISTRO" ] && echoerr "Cannot check for update without DISTRO. Terminating." && exit 1
echoerr "running on distro    : $DISTRO"

# look for update on mounted disks
DATA_DIR=$( getdotenv.sh DATA_DIR "/var/run/ce/" )
UPDATE_FILE="$DISTRO.zip"
echoerr "searching on path    : $DATA_DIR"
echoerr "searching for file   : $UPDATE_FILE"

# find first matching file on possibly mounted USB in our DATA dir
UPDATE_PATH_USB=$( timeout 10s find $DATA_DIR -maxdepth 4 -name "$UPDATE_FILE" -follow -print -quit 2> /dev/null )

if [ -f "$UPDATE_PATH_USB" ]; then    # if file was found on some USB drive
  echoerr "found update on USB  : $UPDATE_PATH_USB"
  echo -n "$UPDATE_PATH_USB"          # output path to USB update
else                                  # update not found on USB, try to search online
  echoerr "found update on USB  : no"
fi
