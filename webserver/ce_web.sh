#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

check_if_pid_running()
{
    # Function checks if the supplied PID from file in $1 is running.

    pid_number=$( cat $1 2> /dev/null )    # get previous PID from file if possible
    found=$( ps -A | grep "$pid_number" | wc -l )       # this will return 1 on PID exists and 0 or total_number_of_processes when PID doesn't exist

    if [ "$found" = "1" ]; then     # found? report 1
        echo "1"
    else                            # not found? report 0
        echo "0"
    fi
}

mkdir -p /var/run/ce/                                   # make this var folder if it doesn't exist
pid_file=/var/run/ce/webserver.pid

app_running=$( check_if_pid_running $pid_file )         # check if PID is still running

if [ "$app_running" = "1" ]; then                       # running? quit now
    echo "Webserver is already running, not starting."
    exit 1
fi

# check if have python3
echo "Checking for python3 presence"
which python3 > /dev/null 2>&1

if [ $? -ne 0 ]; then
  echo "python3 not found"
  exit
fi

# check if found python meets minimum required python version
echo "Checking python3 version"
python_version_ok=$( python3 -c 'import sys;  ver = sys.version_info[:]; print((ver[0] == 3) and (ver[1] >= 5))' | grep True | wc -l )

if [ "$python_version_ok" -eq "0" ]; then
  echo "python3 must have version at least 3.5"
  exit
fi

# if virtualenv doesn't exist, create it
if [ ! -d "./venv/bin" ]; then
  echo "Creating python virtual environment"
  python3 -m venv venv                            # new way to create venv since python 3.6
  . ./venv/bin/activate                           # activate virtualenv

  echo "Installing required packages."
  python3 -m pip install -r requirements.txt      # install requirements
fi

if [ ! -f "./venv/bin/activate" ]; then   # virtualenv still missing?
  echo "python3 virtualenv missing, cannot continue"
  exit
fi

echo "Starting the webserver"
. ./venv/bin/activate       # activate virtualenv

# generate random secret key for flask if it doesn't exist
if [ ! -f "/var/run/ce/flask_secret_key" ]; then
  python -c 'import secrets; print(secrets.token_hex())' > /var/run/ce/flask_secret_key
fi

# start web service
gunicorn --workers 2 --worker-class=gevent --bind 0.0.0.0:80 \
    --access-logformat '%(l)s %(t)s "%(r)s" %(s)s %(b)s %(M)s ms' \
    --access-logfile '-' \
    wsgi:app --pid $pid_file --log-level debug

echo "Webserver has terminated."
