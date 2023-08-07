#!/bin/bash

set -e

export NS_LOG=  # clear enabled logging
# export 'NS_LOG=*=level_all|prefix_func|prefix_time'  # for debug

# Clean-up previous runs. Delete *.txt, *.pcap, *.db files if any.
rm -f *.txt *.pcap *.db

CWD="$PWD"
BASE=$(basename "$PWD")
NS3DIR="/home/networmix/ee500/ns-allinone-3.30/ns-3.30"
cd $NS3DIR

CMD="./waf --cwd=\"$CWD\" --run \"$BASE $@\""
echo "Running: $CMD"
eval $CMD
export NS_LOG=  # clear enabled logging