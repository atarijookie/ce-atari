#!/bin/sh

# count how many times is 'Yocto' is mentioned, if at least once, we're running on Yocto
issue=$( cat /etc/issue | grep -o "Yocto" | wc -l )

# If $issue is 0, we're running on Raspbian - we should probably quit
if [ "$issue" -eq "0" ]; then
	echo "You are running on Raspbian, this script was intended for Yocto, terminating."
    exit
fi

# This is an endless script which checks for wifi dongle, then tries to connect to AP
while :
do
	sleep 5

	# first check if the wifi adapter is present, otherwise there's nothing to do
	gotwifi=$( ifconfig -a | grep wlan0 | wc -l )

	if [ "$gotwifi" -ne 0 ]; then
		echo "Wifi dongle present"
	else
		echo "Wifi dongle missing, skipping"
		continue
	fi

	# now check if the wpa_supplicant is running as daemon, and if not, run it
	gotwpa=$( ps | grep 'wpa_supplicant' | grep -v 'grep' | wc -l )

	if [ "$gotwpa" -eq 0 ]; then
		echo "wpa_supplicant not running, so starting it"
		wpa_supplicant -Dwext -B -iwlan0 -c/etc/wpa_supplicant.conf
		continue				# don't do anything yet in this iteration, let the wpa_supplicant connect
	fi
	
	# check if we're connected to AP
	contoap=$( wpa_cli status | grep 'wpa_state=COMPLETED' | wc -l )

	if [ "$contoap" -eq 0 ]; then
		# if we're not connected to AP, we have to wait until we will be 
		echo "wlan0 is not connected to AP, waiting a while..."
		continue		
	fi

	# check if wlan0 already has an IP 
	gotip=$( ifconfig wlan0 | grep 'inet addr' | wc -l )
	
	if [ "$gotip" -eq 0 ]; then
		# we don't have ip, we should set it - it should get it from DHCP or set it statically 
		echo "wlan0 doesn't have IP, will try to bring it up..."
		ifup wlan0
	else 
		# we have IP, nothing to do
		echo "wlan0 has IP address, nothing to do"
	fi

done

