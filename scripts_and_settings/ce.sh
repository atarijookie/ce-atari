#!/bin/sh 

#
# This script is here to start / stop / get status of all the CosmosEx services and other additional tasks. 
# See help to get more details.
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

get_setting_from_file()
{
    # Get one setting from .cfg file
    # $1 - config variable name, e.g. PID_FILE
    # $2 - path to config file of the service

    RES=$( cat $2 | grep "$1=" )                        # find line with matching KEY ($1)
    RES=$( echo -n $RES | tr -d '\n' | tr -d '\r' )     # remove \n and \r from result
    RES=$( echo -n $RES | cut -f1 -d"#" )               # remove comments starting with '#'
    RES=$( echo -n $RES | cut -d "=" -f 2 )             # leave only VALUE after the '='
    echo -n $RES
}

# get and create data and log dirs as we will need them during the apps start
ENV_FILE=/ce/services/.env
DATA_DIR=$( get_setting_from_file DATA_DIR $ENV_FILE )
LOG_DIR=$( get_setting_from_file LOG_DIR $ENV_FILE )
SETTINGS_DIR=$( get_setting_from_file SETTINGS_DIR $ENV_FILE )

mkdir -p $DATA_DIR
mkdir -p $LOG_DIR
mkdir -p $SETTINGS_DIR

show_help()
{
    echo ""
    echo "This is CosmosEx helper script. Allowed commands are:"
    echo "  start  - start all the required CosmosEx services"
    echo "  stop   - stop all the running CosmosEx services "
    echo "  status - show what CosmosEx services are running"
    echo "  config - run the CosmosEx configuration tool"
    echo "  help   - this help message"
    echo ""
}

# no supported command was used? Show help.
if [ "$1" != "status" ] && [ "$1" != "start" ] && [ "$1" != "stop" ] && \
   [ "$1" != "conf" ] && [ "$1" != "config" ] && \
   [ "$1" != "help" ] && [ "$1" != "--help" ]; then
  show_help
  exit 1
fi

# help was requested? Show help.
if [ "$1" = "help" ] || [ "$1" = "--help" ]; then
  show_help
  exit 0
fi

# Should run the config tool? Run it!
if [ "$1" = "conf" ] || [ "$1" = "config" ]; then
  cd /ce/services/config/
  ./ce_conf.sh
  exit 0
fi

check_if_pid_running()
{
    # Function checks if the supplied PID from file in $1 is running.

    if [ ! -f $1 ]; then            # .pid file non-existent? not running then
        return "0"
    fi

    pid_number=$( cat $1 2> /dev/null )     # get previous PID from file if possible

    if [ -d "/proc/$pid_number/" ]; then    # if directory in /proc exists, process is running (this is faster on RPi than using 'ps' command)
        echo "1"
    else                            # not found? report 0
        echo "0"
    fi
}

handle_service()
{
    # Function handles start/stop/status of service.
    # $1 - command - start | stop | status
    # $2 - path to config file of the service

    pid_file=$( get_setting_from_file PID_FILE $2 )

    # now always check first if the app is running
    app_running=$( check_if_pid_running $pid_file )

    service_dir=$( dirname $2 )                 # directory of the service, e.g. /ce/services/mounter
    service_name=$( basename $service_dir )     # name of the service, deducted from the service dir, e.g.: /ce/services/mounter dir will result in 'mounter'

    # should just report status?
    if [ "$1" = "status" ]; then
        if [ "$app_running" = "1" ]; then
            printf "    %-20s [ UP ]\n" "$service_name"
        else
            printf "    %-20s [    ]\n" "$service_name"
        fi
    fi

    # should start?
    if [ "$1" = "start" ]; then
        exec_cmd=$( get_setting_from_file EXEC_CMD $2 )
        desc_cmd=$( get_setting_from_file DESC_CMD $2 )

        if [ "$app_running" != "1" ]; then          # not running? start
            printf "    %-20s starting\n" "$service_name"
            dir_before=$( pwd )                     # remember current dir
            cd $service_dir                         # change to service dir - so the service will be executed from its own dir - to create file there, to use relative paths to its dir
            eval "$exec_cmd > /dev/null 2>&1 &"     # start the executable file / script file

            if [ ! -z "$desc_cmd" ]; then           # desc_cmd var is set?
                eval "$desc_cmd 2> /dev/null"       # run this command to write description
            fi

            cd $dir_before                          # go back to previous dir
        else
            printf "    %-20s running\n" "$service_name"
        fi
    fi

    # should stop?
    if [ "$1" = "stop" ]; then
        if [ "$app_running" = "1" ]; then       # is running? stop
            printf "    %-20s stopping\n" "$service_name"
            app_pid_number=$( cat $pid_file 2> /dev/null )
            kill -9 "$app_pid_number" > /dev/null 2>&1
        else
            printf "    %-20s not running\n" "$service_name"
        fi
    fi
}

# go through all the service config files and start / stop / status those services
echo ""

if [ "$1" = "start" ]; then
    echo "Starting CosmosEx services:"
elif [ "$1" = "stop" ]; then
    echo "Stopping CosmosEx services:"
elif [ "$1" = "status" ]; then
    echo "CosmosEx services statuses:"
fi

for found in /ce/services/*
do
    if [ -d "$found" ]; then            # if found thing is a dir
        path_cfg="$found/service.cfg"   # construct path to service config file
        just_service=$( basename $found )    # get just name of the service, e.g. 'floppy'

        if [ ! -z $2 ] && [ "$just_service" != "$2" ]; then    # if name of services was provided in argument to this whole command and this is not this service, skip it
            continue
        fi

        if [ -f "$path_cfg" ]; then     # if service config file exists
            handle_service $1 $path_cfg
        fi
    fi
done

echo ""
