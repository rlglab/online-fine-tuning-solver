#!/bin/bash

usage() {
	cat <<- USAGE
	Usage: $0 [ENV_VAR=VALUE]...
	USAGE
}

export CHAT=${CHAT:-NV18:8888} "$@" >/dev/null 2>&1
ENV=(CHAT=$CHAT buffered_input=${buffered_input:-true})
for var in ${@%%=*}; do
	printf "%s\n" "${ENV[@]}" | grep -q "^$var=" || ENV+=("$var=\"${!var}\"")
done
ENV=${ENV[@]}

if tmux has-session -t solver 2>/dev/null; then
	tmux attach -t solver
	exit $?
elif ! who=$(scripts/chat/chat.sh <<< who 2>/dev/null); then
	echo "chat service unavailable!"
	exit 1
elif grep "who:" <<< $who | xargs -n1 2>/dev/null | grep -q $(hostname | sed -E "s/[^0-9]+/b/"); then
	echo "broker already exists!"
	exit 1
fi
for sh in chat/node.sh scripts/{start-container.sh,chat/{broker.sh,worker.sh,trainer.sh}}; do
	if [ ! -e $sh ]; then
		echo "$sh missing!"
		exit 1
	fi
done

tmux new-session \; \
	rename-session "solver" \; \
	rename-window "broker" \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "scripts/chat/broker.sh" C-m \; \
	split-window -h \; \
	select-pane -t 0 \; \
	split-window -v \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "nc ${CHAT/:/ }" C-m \; \
	select-pane -t 2 \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "htop" C-m \; \
	split-window -v \; \
	send-keys "export ${ENV}" C-m \; \
	select-pane -t 1 \; \
	\
	new-window \; \
	rename-window "workers" \; \
	split-window -h \; \
	select-pane -t 0 \; \
	split-window -v \; \
	split-window -v \; \
	select-pane -t 0 \; \
	split-window -v \; \
	select-pane -t 0 \; \
	send-keys "scripts/start-container.sh" C-m \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "sleep 1" C-m \; \
	send-keys "scripts/chat/worker.sh" C-m \; \
	select-pane -t 1 \; \
	send-keys "scripts/start-container.sh" C-m \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "sleep 2" C-m \; \
	send-keys "scripts/chat/worker.sh" C-m \; \
	select-pane -t 2 \; \
	send-keys "scripts/start-container.sh" C-m \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "sleep 3" C-m \; \
	send-keys "scripts/chat/worker.sh" C-m \; \
	select-pane -t 3 \; \
	send-keys "scripts/start-container.sh" C-m \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "sleep 4" C-m \; \
	send-keys "scripts/chat/worker.sh" C-m \; \
	select-pane -t 4 \; \
	send-keys "htop" C-m \; \
	split-window -v -p 75 \; \
	send-keys "watch -n1 nvidia-smi" C-m \; \
	\
	new-window \; \
	rename-window "trainer" \; \
	split-window -h \; \
	select-pane -t 0 \; \
	split-window -v -p 66 \; \
	split-window -v -p 50 \; \
	select-pane -t 0 \; \
	send-keys "scripts/start-container.sh" C-m \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "sleep 5" C-m \; \
	send-keys "scripts/chat/trainer.sh" C-m \; \
	select-pane -t 1 \; \
	send-keys "scripts/start-container.sh" C-m \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "sleep 6" C-m \; \
	send-keys "scripts/chat/trainer.sh sp" C-m \; \
	select-pane -t 2 \; \
	send-keys "scripts/start-container.sh" C-m \; \
	send-keys "export ${ENV}" C-m \; \
	send-keys "sleep 7" C-m \; \
	send-keys "scripts/chat/trainer.sh op" C-m \; \
	select-pane -t 3 \; \
	send-keys "htop" C-m \; \
	split-window -v -p 75 \; \
	send-keys "watch -n1 nvidia-smi" C-m \; \
	\
	select-window -t 0 \; \
;
