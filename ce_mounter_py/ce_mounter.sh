#!/bin/sh

# check if running as root, and if not, execute this script with sudo
if [ $(id -u) != 0 ]; then
  sudo $0 "$@"
  exit 0
fi

# check if got fuse-zip
got_fuse_zip=$( which fuse-zip )
if [ $? -ne 0 ]; then
  echo "fuse-zip not found"
  exit
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

hname=$( hostname )           # get hostname into variable, as $HOSTNAME seems to be missing in some cases
venv_dir="venv_$hname"        # venv dir with hostname so multiple venvs can be present when this folder is shared

# if virtualenv doesn't exist, create it
if [ ! -d "./$venv_dir/bin" ]; then
  echo "Creating python virtual environment"
  python3 -m venv $venv_dir                       # new way to create venv since python 3.6
  . ./$venv_dir/bin/activate                      # activate virtualenv

  echo "Installing required packages."
  python3 -m pip install -r requirements.txt      # install requirements
fi

if [ ! -f "./$venv_dir/bin/activate" ]; then      # virtualenv still missing?
  echo "python3 virtualenv missing, cannot continue"
  exit
fi

echo "Starting the app"
. ./$venv_dir/bin/activate      # activate virtualenv
python main.py                  # start the app
echo "App has terminated."
