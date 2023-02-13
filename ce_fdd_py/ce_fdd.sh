#!/bin/sh

# check if running as root
if [ $(id -u) != 0 ]; then
  echo "Please run this as root"
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
fi

if [ ! -f "./$venv_dir/bin/activate" ]; then      # virtualenv still missing?
  echo "python3 virtualenv missing, cannot continue"
  exit
fi

. ./$venv_dir/bin/activate                        # activate virtualenv

echo "Checking if all required packages are installed"
python3 -c "import pkg_resources; pkg_resources.require(open('requirements.txt',mode='r'))" > /dev/null 2>&1

if [ $? -ne 0 ]; then                             # the above command failed, so something is missing
  echo "Some package missing, installing required packages..."
  python3 -m pip install -r requirements.txt      # install requirements
else
  echo "All packages present."
fi

echo "Starting the app"
. ./$venv_dir/bin/activate    # activate virtualenv
python main.py                # start the app
echo "App has terminated."
