#!/bin/bash

usage() {
	cat <<- USAGE
	Usage: $0 [CHAT=HOST:PORT]
	USAGE
}

broker=$(hostname | sed -E "s/[^0-9]+/b/")
worker=$(hostname)
trainer=$(hostname | sed -E "s/[^0-9]+/LL/")

export CHAT=${CHAT:-NV18:8888} "$@" >/dev/null 2>&1
{
	printf "%s << solver kill\n" $worker-0 $worker-1 $worker-2 $worker-3
	printf "%s << stop\n" $trainer $trainer-sp $trainer-op
	sleep 10
	printf "%s << operate shutdown\n" $broker $worker-0 $worker-1 $worker-2 $worker-3 $trainer $trainer-sp $trainer-op

} | scripts/chat/chat.sh
