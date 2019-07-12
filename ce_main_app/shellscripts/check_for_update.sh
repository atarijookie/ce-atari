#!/bin/sh

echo " "
echo "Checking for update..."

distro=$( /ce/whichdistro.sh )
url_to_git_repo="https://github.com/atarijookie/ce-atari-releases.git"
path_to_repo_archive="https://github.com/atarijookie/ce-atari-releases/archive/"
path_to_zip_update="$path_to_repo_archive$distro.zip"
path_to_tmp_update="/tmp/$distro.zip"

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
got_git=$( git --version 2>/dev/null | grep 'git version' | wc -l )

if [ "$got_git" -eq "0" ]; then         # if don't have git, then run the update, which will try to install git
    echo "don't have git, do update"
    touch /tmp/UPDATE_PENDING_YES
    exit
fi

echo "checking for new files"

cd /ce/                             # go to /ce directory
output=$( git remote update )       # try git remote update
output=$( echo $output | grep 'Not a git repo' | wc -l )    # check if git complained that this is not a repo

if [ "$output" -gt "0" ]; then      # git complained that this is not a repo? run the update
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
