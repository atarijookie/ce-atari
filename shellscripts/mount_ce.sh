#!/bin/sh

# Use this script on linux device used for development, so you will have the same mapping of folders like
# on your main development / storage server.

SERVER_ADDR='192.168.123.55'
USER='jookie'

sudo mkdir -p /ce
#sudo mount -t cifs -o username=$USER //$SERVER_ADDR/ce /ce
sudo mount -t nfs4 $SERVER_ADDR:/ce /ce

sudo mkdir -p /shared
#sudo mount -t cifs -o username=$USER //$SERVER_ADDR/shared_writable /shared
sudo mount -t nfs4 $SERVER_ADDR:/shared /shared

