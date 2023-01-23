#!/bin/sh

# Use this script on linux device used for development, so you will have the same mapping of folders like
# on your main development / storage server.

sudo mkdir -p /ce
#sudo mount -t cifs -o username=jookie //192.168.123.55/ce /ce
sudo mount -t nfs4 192.168.123.55:/ce /ce

sudo mkdir -p /shared
#sudo mount -t cifs -o username=jookie //192.168.123.55/shared_writable /shared
sudo mount -t nfs4 192.168.123.55:/shared /shared

