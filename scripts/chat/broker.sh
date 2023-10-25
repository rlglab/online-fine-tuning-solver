#!/bin/bash
NODE=$(find -L chat/ ./ -maxdepth 2 -name node.sh -type f -executable | head -n1)
[[ $NODE ]] || { echo "node.sh script not found!"; exit 1; }
broker=${1:-$(hostname | sed -E "s/[^0-9]+/b/")}
logfile=${LOGFILE:-/tmp/broker-$(hostname)-$(date '+%Y%m%d-%H%M%S').log}
$NODE ${CHAT:-NV18:8888} name=$broker mode=broker plugins=scripts/solver-wrapper.sh logfile=$logfile system_tick=0.01 hold_timeout=10000
