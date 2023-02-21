#!/bin/sh 

#
# This script is here to start / stop / get status of all the CosmosEx services and other additional tasks. 
# See help to get more details.
#
# $1 - command name (e.g. start, stop, ...)
# $2 - optional service name, if we want to manipulate only one service
#

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

# if symlink to this script for users doesn't exist, create it
if [ ! -L /usr/local/bin/ce ]; then
    ln -fs $0 /usr/local/bin/ce
fi

# get and create data and log dirs as we will need them during the apps start
PATH=$PATH:/ce/update     # add our update folder to path so we can use relative paths when referencing our scripts

DATA_DIR=$( getdotenv.sh DATA_DIR "" )
LOG_DIR=$( getdotenv.sh LOG_DIR "" )
SETTINGS_DIR=$( getdotenv.sh SETTINGS_DIR "" )

mkdir -p $DATA_DIR
mkdir -p $LOG_DIR
mkdir -p $SETTINGS_DIR

show_help()
{
    echo ""
    echo "CosmosEx helper script."
    echo "Usage: ce command [options]"
    echo ""
    echo "Commands:"
    echo "  u, start   - start all the required CosmosEx services"
    echo "  d, stop    - stop all the running CosmosEx services "
    echo "  r, restart - restart all the required CosmosEx services"
    echo "  s, status  - show what CosmosEx services are running"
    echo "  c, config  - run the CosmosEx configuration tool"
    echo "     update  - download and apply the latest update from internet"
    echo "               (this also flashes the chips IF NEEDED)"
    echo "     upforce - same as update, but FLASHING CHIPS IS FORCED"
    echo "  ?, help    - this help message"
    echo ""
    echo "Options:"
    echo "   [name of service] - start / stop / restart only service with this name"
    echo ""
}

# no supported command was used? Show help.
if [ "$1" != "status" ] && [ "$1" != "s" ] && \
   [ "$1" != "start" ] && [ "$1" != "u" ] && \
   [ "$1" != "restart" ] && [ "$1" != "r" ] && \
   [ "$1" != "stop" ] && [ "$1" != "d" ] && \
   [ "$1" != "conf" ] && [ "$1" != "config" ] && [ "$1" != "c" ] && \
   [ "$1" != "update" ] && [ "$1" != "upforce" ] && \
   [ "$1" != "help" ] && [ "$1" != "--help" ] && [ "$1" != "?" ]; then
  show_help
  exit 1
fi

# help was requested? Show help.
if [ "$1" = "help" ] || [ "$1" = "--help" ] || [ "$1" = "?" ]; then
  show_help
  exit 0
fi

# Should run the config tool? Run it!
if [ "$1" = "conf" ] || [ "$1" = "config" ] || [ "$1" = "c" ]; then
  cd /ce/services/config/
  ./ce_conf.sh
  exit 0
fi

# Should run the config tool? Run it!
if [ "$1" = "update" ] || [ "$1" = "upforce" ]; then
  if [ "$1" = "upforce" ]; then
    echo "Starting UPDATE with FORCED CHIPS FLASHING!"
    touch /tmp/FW_FLASH_ALL     # create this file, so ce_update.sh will do flashing in any case
  else
    echo "Starting UPDATE with flashing chips only when needed."
  fi

  CE_UPDATE_TRIGGER=$( getdotenv.sh CE_UPDATE_TRIGGER "" )
  echo "Created CE_UPDATE_TRIGGER file: $CE_UPDATE_TRIGGER"
  echo "This should trigger CE update within few seconds."
  touch "$CE_UPDATE_TRIGGER"

  exit 0
fi

handle_service()
{
    # Function handles start/stop/status of service.
    # $1 - command - start | stop | status | restart
    # $2 - service name with extension == systemd unit service file (e.g. 'ce_core.service')
    # $3 - service name without 3 characters prefix and without extension (e.g. 'core')

    app_running=$( systemctl status $2 | grep -c 'Active: active' )

    # should just report status?
    if [ "$1" = "status" ]; then
        if [ "$app_running" != "0" ]; then
            printf "    %-20s [ UP ]\n" "$3"
        else
            printf "    %-20s [    ]\n" "$3"
        fi
    fi

    # should start?
    if [ "$1" = "start" ]; then
        if [ "$app_running" = "0" ]; then          # not running? start
            printf "    %-20s starting\n" "$3"
            systemctl start $2
        else
            printf "    %-20s running\n" "$3"
        fi
    fi

    # should stop?
    if [ "$1" = "stop" ]; then
        if [ "$app_running" != "0" ]; then       # is running? stop
            printf "    %-20s stopping\n" "$3"
            systemctl stop $2
        else
            printf "    %-20s not running\n" "$3"
        fi
    fi

    # should restart?
    if [ "$1" = "restart" ]; then
        printf "    %-20s restarting\n" "$3"
        systemctl restart $2
    fi
}

# go through all the service files and start / stop / status those services
echo ""

if [ "$1" = "start" ] || [ "$1" = "u" ]; then
    echo "Starting CosmosEx services:"
    action="start"
elif [ "$1" = "restart" ] || [ "$1" = "r" ]; then
    echo "Restarting CosmosEx services:"
    action="restart"
elif [ "$1" = "stop" ] || [ "$1" = "d" ]; then
    echo "Stopping CosmosEx services:"
    action="stop"
elif [ "$1" = "status" ] || [ "$1" = "s" ]; then
    echo "CosmosEx services statuses:"
    action="status"
fi

for srvc in /ce/systemd/*.service             # find all the services
do
  srvc=$( basename $srvc )                          # get filename with extension
  srvc_no_ext=$( basename $srvc .service )          # get filename without extension
  srvc_no_pref=$( echo $srvc_no_ext | cut -c 4- )   # get filename without extension and without first 3 chars

  if [ ! -z $2 ]; then                      # if service name was provided
    if [ "$srvc_no_ext" != "$2" ] && [ "$srvc_no_pref" != "$2" ]; then    # if this service is not service filename without extension or without first 3 chars ('ce_'), skip it
      continue
    fi
  fi

  handle_service $action $srvc $srvc_no_pref
done

echo ""
