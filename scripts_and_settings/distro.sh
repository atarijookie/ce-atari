#!/bin/sh

# This script return the distro of linux which it is run on. Instead of guessing it like before
# it's just reading it from .env file so you can override it if needed, and if that fails,
# the user is asked for manual input of the distro name (in case it's not in the .env file)

echoerr() { echo "$@" 1>&2; }

DISTRO=$( getdotenv.sh DISTRO "" )

# no DISTRO from .env file? try getting it manually
if [ -z "$DISTRO" ]; then
  echoerr "Failed to get DISTRO from .env"
  echoerr "Please specify DISTRO manually:"
  read DISTRO
fi

echo "$DISTRO"      # output DISTRO to stdout
