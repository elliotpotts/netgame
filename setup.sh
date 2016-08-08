#!/usr/bin/env sh
for ((i=0; i<=5; i++)) do
    ip addr add 192.168.1.10$i brd + dev wlan1 label wlan1:virt$i
done
