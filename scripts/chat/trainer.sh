#!/bin/bash
if [[ $1 =~ ^(|sp|op)$ ]]; then
	name=$(hostname | sed -E "s/[^0-9]+/${TRAINER_LINK_PREFIX:-LL}/")${1:+-$1}
else
	name=$1
fi
cmd=${@:2}

CHAT=${CHAT:-NV18:8888}
exec {tcp}<>/dev/tcp/${CHAT/://} || exit $?
input() {
	if read -r -t 0 -u ${tcp}; then
		IFS= read -r -u ${tcp} ${1:-message}
		return $?
	else
		sleep ${system_tick:-1}
		eval ${1:-message}=
		return 0
	fi
}
output() {
	echo "$@" >&$tcp
}

kill_cmd() {
	#local pid
	#for pid in $(pgrep -P $1); do kill_cmd $pid; done
	#pkill -P $1
	#kill $1
	trap '' TERM
	kill -TERM 0
	trap - TERM
}

output name $name
while input message; do
	if [[ $message == *" >> start" ]]; then
		[[ $pid ]] && kill_cmd $pid 2>/dev/null
		echo "==== start $cmd ====" | tee -a $name.log
		eval "$cmd" 2>&1 | tee -a $name.log &
		pid=$!
	elif [[ $message == *" >> stop" ]]; then
		[[ $pid ]] && kill_cmd $pid 2>/dev/null
		echo "==== stop $cmd ====" | tee -a $name.log
		pid=
	elif [[ $message == *" >> set "* ]]; then
		cmd=${message#* >> set }
		echo "==== set $cmd ====" | tee -a $name.log
	elif [[ $message == *" >> exit" || $message == *" >> operate shutdown" ]]; then
		echo "==== exit ===="
		break
	elif [[ $message == "%"* ]]; then
		echo "==== info ${message#* } ===="
	fi
done
