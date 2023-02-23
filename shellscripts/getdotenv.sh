#!/bin/bash

# Script to get value of variable from .env file. This works by sourcing the .env file,
# and this works in BASH but not in /bin/sh, so the shebang above is expected to be /bin/bash
# If you ever will find yourself in situation where using BASH for this is not possible,
# find a different way of fetching the value from .env and implement it here.
# All other scripts are reading .env file using this script, so this is the only place you need to change in that case.
#
# Arguments:
# $1 - config variable name, e.g. PID_FILE
# $2 - default value if value couldn't be retrieved from .env file

ENV_FILE=/ce/services/.env                      # specify path to env file
source $ENV_FILE                                # source all variables from .env file
RES="${!1}"                                     # into variable RES fetch content of variable, who's name is in $1
[ -z "$RES" ] && echo -n $2 || echo -n $RES     # if $RES is empty, return default, otherwise return $RES
