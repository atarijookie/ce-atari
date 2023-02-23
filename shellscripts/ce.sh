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

# function to show help
show_help()
{
cat << EOF

CosmosEx helper script.
Usage: ce command [options]

Commands:
  u, start   - start all the required CosmosEx services
  d, stop    - stop all the running CosmosEx services
  r, restart - restart all the required CosmosEx services
  s, status  - show what CosmosEx services are running
  c, config  - run the CosmosEx configuration tool
     update  - download and apply the latest update from internet
               (this also flashes the chips IF NEEDED)
     upforce - same as update, but FLASHING CHIPS IS FORCED
  ?, help    - this help message

Options:
   [name of service] - start / stop / restart only service with this name

EOF
}

# function to start the update of CE software
start_update()
{
  if [ "$1" = "upforce" ]; then
    echo "Starting UPDATE with FORCED CHIPS FLASHING!"
    CE_FLASH_ALL=$( getdotenv.sh CE_FLASH_ALL "" )
    touch "$CE_FLASH_ALL"     # create this file, so ce_update.sh will do flashing in any case
    echo "Created CE_FLASH_ALL file: $CE_FLASH_ALL"
  else
    echo "Starting UPDATE with flashing chips only when needed."
  fi

  CE_UPDATE_TRIGGER=$( getdotenv.sh CE_UPDATE_TRIGGER "" )
  touch "$CE_UPDATE_TRIGGER"
  echo "Created CE_UPDATE_TRIGGER file: $CE_UPDATE_TRIGGER"
  echo "This should trigger CE update within few seconds."
}

# handle some of the commands here, let other supported commands pass and unhandled commands show just help
case $1 in
  status|s|start|u|restart|r|stop|d) ;;                                     # supported command - do nothing
  conf|config|c)      /ce/services/config//ce_conf.sh ;         exit 0;;    # conf tool
  update|upforce)     start_update $1 ;                         exit 0;;    # start the update
  ""|help|?|--help)                                show_help ;  exit 0;;    # no command or help command
  *)                  echo "unknown command: $1" ; show_help ;  exit 1;;    # unknown command
esac

handle_service()
{
    # Function handles start/stop/status of service.
    # $1 - command - start | stop | status | restart
    # $2 - service name with extension == systemd unit service file (e.g. 'ce_core.service')
    # $3 - service name without 3 characters prefix and without extension (e.g. 'core')

    status=$( systemctl status $2 2>&1 )                        # get service status

    needs_reload=$( echo $status | grep -c 'daemon-reload' )    # if this was found in the systemctl output, we need to do daemon-reload
    [ "$needs_reload" != "0" ] && systemctl daemon-reload       # if systemctl needs reload, do it

    app_running=$( echo $status | grep -c 'Active: active' )    # check if app is running

    # should just report status?
    if [ "$1" = "status" ]; then
      [ "$app_running" != "0" ] && stat='UP' || stat='  '
      printf "    %-20s [ $stat ]\n" "$3"
      return      # exit function here, nothing more to do
    fi

    case $1 in
      start)    [ "$app_running"  = "0" ] && act='starting' || act='running'      ;;
      stop)     [ "$app_running" != "0" ] && act='stopping' || act='not running'  ;;
      restart)                               act='restarting'                     ;;
      *)        return ;;               # other commands not supported
    esac

    printf "    %-20s $act\n" "$3"      # show action to user
    systemctl $1 $2                     # do the action
}

echo ""

# show appropriate action message, convert short action to long action string (e.g. 's' -> 'status')
case $1 in
  status|s)   echo "CosmosEx services statuses:"    ; action="status"   ;;
  start|u)    echo "Starting CosmosEx services:"    ; action="start"    ;;
  restart|r)  echo "Restarting CosmosEx services:"  ; action="restart"  ;;
  stop|d)     echo "Stopping CosmosEx services:"    ; action="stop"     ;;
  *)          echo "unknown command: $1"            ; exit 1            ;;
esac

# go through all the service files and start / stop / status those services
for srvc in /ce/systemd/*.service             # find all the services
do
  srvc=$( basename $srvc )                          # get filename with extension           e.g. /ce/systemd/ce_core.service -> ce_core.service
  srvc_no_ext=$( basename $srvc .service )          # get filename without extension                    e.g. ce_core.service -> ce_core
  srvc_no_pref=$( echo $srvc_no_ext | cut -c 4- )   # get filename without extension and without first 3 chars, e.g. ce_core -> core

  # if service name was provided and this service is not that wanted service, skip it
  # We are comparing wanted service name to:
  # - service filename without extension - e.g. with 'ce_core'
  # - service filename without extension and without first 3 chars - e.g. with 'core'
  if [ ! -z $2 ] && [ "$srvc_no_ext" != "$2" ] && [ "$srvc_no_pref" != "$2" ]; then
    continue
  fi

  handle_service $action $srvc $srvc_no_pref
done

echo ""
