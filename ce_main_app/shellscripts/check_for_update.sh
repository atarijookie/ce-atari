#!/bin/sh

echo " "
echo "Checking for update..."

distro=$( /ce/whichdistro.sh )

# delete both update_pending answer files
rm -f /tmp/UPDATE_PENDING_YES
rm -f /tmp/UPDATE_PENDING_NO

# on yocto - not actually checking, just do the update
if [ "$distro" = "yocto" ]; then
    echo "on Yocto - always do update"
    touch /tmp/UPDATE_PENDING_YES
    exit
fi

# check if got git installed
git --version

if [ "$?" -ne "0" ]; then           # if don't have git, then run the update, which will try to install git
    echo "don't have git, do update"
    touch /tmp/UPDATE_PENDING_YES
    exit
fi

echo "checking for new files"

cd /ce/                             # go to /ce directory
git remote update                   # try git remote update

if [ "$?" -ne "0" ]; then           # git complained that this is not a repo? run the update
    echo "/ce/ is not a git repo yet, do update"
    touch /tmp/UPDATE_PENDING_YES
    exit
fi

# check if we're behind repo or just fine
output=$( git status -uno | grep 'behind' | wc -l )

if [ "$output" -gt "0" ]; then      # if 'behind' was found in the status
    echo "we're behind remote repo, do update"
    touch /tmp/UPDATE_PENDING_YES
else                                # not behind, no update pending
    echo "no changes found, don't do update"
    touch /tmp/UPDATE_PENDING_NO
fi
