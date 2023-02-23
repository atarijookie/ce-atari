#!/bin/sh

# get log path, generate current date and logfile with that date, redirect output to log and console
PATH=$PATH:/ce/update       # add our update folder to path so we can use relative paths when referencing our scripts
LOG_DIR=$( getdotenv.sh LOG_DIR "/var/log/ce" )
LOG_FILE="$LOG_DIR/updater_service.log"
{
CE_UPDATE_TRIGGER=$( getdotenv.sh CE_UPDATE_TRIGGER "" )   # if this file appears, we should do the update
CE_UPDATE_RUNNING=$( getdotenv.sh CE_UPDATE_RUNNING "" )   # if this file is present, we are doing update

services_stop()
{
  echo "CosmosEx update - stopping CE services"
  for srvc in /ce/systemd/*.service         # find all the services
  do
    srvc=$( basename $srvc )                # get just filename

    if [ "$srvc" != "ce_updater.service" ]; then      # make sure we're not stopping this updater service
      echo "CosmosEx update - stopping $srvc"
      systemctl stop $srvc                            # stop that other service (but not updater!)
    fi
  done
}

services_start()
{
  echo "CosmosEx update - symlinking CE services"
  for srvc in /ce/systemd/*.service         # find all the services
  do
    srvc=$( basename $srvc )                # get just filename
    echo "CosmosEx update - symlinking $srvc"
    ln -fs /ce/systemd/$srvc /etc/systemd/system/$srvc
  done

  echo "CosmosEx update - reloading all services"
  systemctl daemon-reload

  echo "CosmosEx update - starting CE services"
  for srvc in /ce/systemd/*.service         # find all the services
  do
    srvc=$( basename $srvc )                # get just filename
    echo "CosmosEx update - enabling and starting $srvc"
    systemctl enable $srvc                  # enable autostart of service
    systemctl start $srvc                   # start that service
  done
}

# This is the main loop of updater service. It only waits for update trigger file to appear, then it stops
# all the other CE services, runs the update script, then symlinks all the systemd service units and starts them,
# and then waits some more.

while :
do
  if [ -f "$CE_UPDATE_TRIGGER" ]; then        # if trigger file exists
    echo " "
    rm -f "$CE_UPDATE_TRIGGER"                # delete that file so we won't be doing update in endless loop
    touch "$CE_UPDATE_RUNNING"                # doing update now

    services_stop                             # stop all services

    echo "CosmosEx update - starting update"  # echo that we're starting
    /ce/update/ce_update.sh                   # execute the actual update script
    echo "CosmosEx update - finished update"

    services_start                            # start all services

    rm -f "$CE_UPDATE_TRIGGER"                # delete update trigger file if it appeared during the install
    rm -f "$CE_UPDATE_RUNNING"                # not doing update now
  else                                        # no trigger file? just wait some more
    rm -f "$CE_UPDATE_RUNNING"                # not doing update now
    sleep 3
    echo -n "."
  fi
done

} 2>&1 | tee $LOG_FILE    # end of redirect to file and console
