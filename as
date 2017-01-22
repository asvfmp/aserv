#!/bin/bash

tmux new-session -d -s ASERV
tmux rename-window 'syncaserv'
tmux select-window -t ASERV:0
tmux split-window -h

tmux select-pane -t ASERV:0.1
tmux send-keys 'sudo ./syncaserv' 'C-m'
tmux split-window -v -t 1
tmux select-pane -t ASERV:0.2
tmux send-keys 'tail -f /var/log/messages' 'C-m'
tmux select-pane -t 0
T=`ip route list |sed -n 's/^default.*metric //p'`
dm=10000
for ir in ${T}
do
    if [ "$ir" -lt "$dm" ]; then dm=$ir; fi
done
__iface=`ip route list |grep -E "^default.*metric $dm" |sed -n 's/.*dev //;s/ metric.*//p'`
#tmux send-keys "sudo iftop -i $__iface" C-m
tmux select-pane -t ASERV:0.0
tmux split-window -v
tmux select-pane -t ASERV:0.1
