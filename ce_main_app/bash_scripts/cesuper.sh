#!/bin/sh
while :
do

    #try to get pid of process
    pid=$(pidof cosmosex)

    if [ -z "$pid" ]            # if pid of process is empty, process not running
    then

        # app is not running - either we want to do update, or it crashed

        if [ -e /ce/update/doupdate.sh ]        # if update file exists, we do the update
        then
            echo "doing update"

            chmod 777 /ce/update/doupdate.sh    # make the script executable
            /ce/update/doupdate.sh              # execute the update script, wait for finish
            rm -f /ce/update/doupdate.sh        # delete the update script
        fi

        # if we got here, there was an update or crash, so run the app!
        echo "restarting app"
        /ce/app/cosmosex &                      # run the application on the background

    else                        # if pid of process is not empty, process is running

        # app is running, just sleep and check status later
        echo "app is running, sleeping"

    fi

    sleep 5

done

