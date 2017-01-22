#!/bin/bash
COMP=
for itty in $(ls /dev/ttyUSB*)
    do
	udevadm info -a -n $itty |grep -q "CarDuino"                                                                                            
	if [[ $? -eq 0 ]]; then COMP=$itty; break; fi                                                                                  
    done                                                                                                                                

if [[ -n $COMP ]]
then                                                                                                                  
	echo "CarDuino connected to port: $COMP"
    else
	echo "[sync_serv]: Connection module don't connected. Server going down."
	exit 1
fi
