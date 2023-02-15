#!/bin/sh

# Script to get value of variable from .env file
# Fetching value in this shell version might not be as flexible as the original .env supports,
# so for .env values which you plan to fetch in shell, you should:
# - avoid comments in that line and also try to avoid trailing whitespaces
# - avoid referencing other variables (e.g. DONT=DO/${THIS} )
#
# Arguments:
# $1 - config variable name, e.g. PID_FILE
# $2 - default value if value couldn't be retrieved from .env file
#

get_setting_from_file()
{
    # Get one setting from .env file
    # $1 - config variable name, e.g. PID_FILE
    # $2 - path to config file of the service
    # $3 - default value if value couldn't be retrieved from .env file

    RES=$( cat $2 | grep "$1=" )                        # find line with matching KEY ($1)
    RES=$( echo -n $RES | tr -d '\n' | tr -d '\r' )     # remove \n and \r from result
    RES=$( echo -n $RES | cut -f1 -d"#" )               # remove comments starting with '#'
    RES=$( echo -n $RES | cut -d "=" -f 2 )             # leave only VALUE after the '='

    if [ -z "$RES" ]; then  # value empty? use default ($3)
      echo -n $3
    else                    # value present? use it
      echo -n $RES
    fi
}

ENV_FILE=/ce/services/.env              # specify path to env file
get_setting_from_file $1 $ENV_FILE $2   # fetch it from file and echo it
