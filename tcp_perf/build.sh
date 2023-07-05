#!/bin/sh

echo "building server"
g++ server.cpp -o server.elf

echo "building client"
g++ client.cpp -o client.elf
