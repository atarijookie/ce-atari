#!/bin/sh

# This script return the distro of linux which it is run on. Instead of guessing it like before
# it's just reading it from .env file so you can override it if needed.

echoerr() { echo "$@" 1>&2; }

[ ! -f /ce/services/.env ] && echo ".env file not found!" && exit 1
. /ce/services/.env       # source env variables

# no DISTRO from .env file? fail
if [ -z "$DISTRO" ]; then
  echoerr "Failed to get DISTRO from .env"
  exit 1
fi

echo "$DISTRO"      # output DISTRO to stdout
