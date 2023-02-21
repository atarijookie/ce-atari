#!/bin/sh

# get log path, generate current date and logfile with that date, redirect output to log and console
LOG_DIR=$( /ce/update/getdotenv.sh LOG_DIR "/var/log/ce" )
NOW=$( date +"%Y%m%d_%H%M%S" )
LOG_FILE="$LOG_DIR/updater_service_$NOW.log"
{
CE_UPDATE_TRIGGER=$( /ce/update/getdotenv.sh CE_UPDATE_TRIGGER "" )

while :
do
  if [ -f "$CE_UPDATE_TRIGGER" ]; then        # if trigger file exists
    echo " "
    rm -f "$CE_UPDATE_TRIGGER"                # delete that file so we won't be doing update in endless loop

    echo "CosmosEx update - stopping CE services"
    for srvc in /ce/systemd/*.service         # find all the services
    do
      srvc=$( basename $srvc )                # get just filename

      if [ "$srvc" != "ce_updater.service" ]; then      # make sure we're not stopping this updater service
        echo "CosmosEx update - stopping $srvc"
        systemctl stop $srvc                            # stop that other service (but not updater!)
      fi
    done

    echo "CosmosEx update - starting update"  # echo that we're starting
    /ce/update/ce_update.sh                   # execute the actual update script
    echo "CosmosEx update - finished update"

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

    rm -f "$CE_UPDATE_TRIGGER"                # delete update trigger file if it appeared during the install
  else                                        # no trigger file? just wait some more
    sleep 3
    echo -n "."
  fi
done

} 2>&1 | tee $LOG_FILE    # end of redirect to file and console
