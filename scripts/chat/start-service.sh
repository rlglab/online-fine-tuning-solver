#!/bin/bash
chat=$(find -L chat/ ./ -maxdepth 2 -name chat -type f -executable | head -n1)
[[ $chat ]] || { echo "chat binary not found!"; exit 1; }
port=${1:-8888}
logz=chat-$(hostname)-$(date +%Y%m%d).log.gz
echo "$chat $port 2>&1 | gzip > $logz"
$chat $port 2>&1 | gzip > $logz
