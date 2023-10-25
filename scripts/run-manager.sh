#!/bin/bash

usage() {
	cat <<- USAGE
	Usage: $0 [GAME] [OPENING]... [CONFIG]...
	Launch the baseline or the online fine-tuning solver to solve opening problems.
	E.g.,  $0 killallgo ID1 ID2 ID3 BROKER=b16 SIM=1000000 ONLINE=false
	       $0 killallgo ID1 BROKER=b16 ONLINE=true CUDA_VISIBLE_DEVICES=1

	The results of each opening problem are saved using label:
	[OPENING_ID]-[SOLVER_MODE]-[NUM_SIMULATION]-[PCN_THRESHOLD]-[ENV_LABEL]

	Note that this script does not start chat service and workers,
	the required environment should be configured before running this script.
	
	Options:
	  GAME     game type, e.g., "killallgo"
	  OPENING  ID of opening problems to be solved
	           IDs and mapped SGFs are defined in .opening.txt
	  CONFIG   configuration for the solver system, specified with KEY=VALUE
	           lowercase keys are manager configurations (passed to manager using -conf_str)
	           uppercase keys are common/manager configurations, listed below
	
	Configurations:
	  CHAT     chat service, default NV18:8888
	  CUDA_VISIBLE_DEVICES  available GPU devices, default 0
	  BROKER   broker name, equivalent to broker_name
	  SIM      manager simulation count, equivalent to actor_num_simulation, default 5000000
	  PCN      manager PCN threshold, equivalent to manager_pcn_value_threshold, default 16.5
	  ONLINE   use online fine-tuning or not, equivalent to use_online_fine_tuning
	  NN_HOME  home folder for network models, default training
	  MODEL    initial network, specified with NAME:ITER, default gpcn_n32_m16_empty_op:100000
	           this sets nn_file_name to \${NN_HOME}/\${NAME}/model/weight_iter_\${ITER}.pt
	  RUN      executable file name for the manager, default \${GAME}_solver
	  CFG      configuration file for the manager, default config/manager.cfg
	  TRAINER_CFG  configuration file for the trainer, default config/trainer.cfg
	  TIMEOUT  timeout in seconds for solving, default 0 (unlimited)
	  OVERWRITE  overwrite existing results or not, default false
	           to run multiple trials with the same settings, set \${INFO} to something else
	  DRYRUN   print the command without actually launching, default false
	  GDB      launch the manager using GDB, default false
	  TAG      prefix for result label
	  INFO     suffix for result label
	
	Some keys are aliases for other keys, e.g., BROKER=b16 is equivalent to broker_name=b16.
	For detailed default configurations and key aliases, check out this script.

	USAGE
}

CONFIG=( # builtin defaults
	CHAT=NV18:8888
	CUDA_VISIBLE_DEVICES=0
	BROKER=broker
	SIM=5000000
	NN_HOME=training
	MODEL=gpcn_n32_m16_empty_op:100000
	PCN=16.5
	GAME=killallgo
	CFG=config/manager.cfg
	TRAINER_CFG=config/trainer.cfg
	OVERWRITE=false
)

ALIAS=( # alias for long config names
	CVD=CUDA_VISIBLE_DEVICES
	ONLINE=use_online_fine_tuning
	online=use_online_fine_tuning
	SIM=actor_num_simulation
	PCN=manager_pcn_value_threshold
	GHI=use_ghi_check
	VS=use_virtual_solved
	AO=manager_send_and_player_job
	TOPK=manager_top_k_selection
	BROKER=broker_name
	broker=broker_name
	model=MODEL
	game_type=GAME
	game=GAME
	conf_file=CFG
	trainer_conf_file=TRAINER_CFG
)

# fetch env variables for all builtin defaults
CONFIG+=($(env | grep -E "^($(tr ' ' '|' <<< ${CONFIG[@]%%=*}))="))
# fetch CFG FLAGS conf_str
eval $(printf "%s\n" "${CONFIG[@]}" "${@#-}" | grep -E "^(CFG|FLAGS|conf_str)=") 2>/dev/null
CONFIG=$({ # print configs with priority and fix aliases
	printf "%s\n" "${CONFIG[@]}"
	tr ':' '\n' <<< ${FLAGS}:${conf_str}
	printf "%s\n" "${@#-}" | grep = | grep -Ev "^(FLAGS|conf_str)="
} | grep . | sed $(printf -- "-e s/^%s=/g\n" "${ALIAS[@]//=/=/}") -e 's/=/="/' -e 's/$/"/')

# configuration priority: (highest) cmdline > env variables > defaults > config file (lowest)
source "$CFG" 2>/dev/null # load config file
eval "$CONFIG" 2>/dev/null # load cmdline arguments, env variables, and built-in defaults
export CHAT CUDA_VISIBLE_DEVICES

# add all lowercase configs into conf_str
FLAGS=$(for conf in $(sed -E 's/=.+//' <<< $CONFIG | grep "^[a-z0-9]" | awk '!x[$0]++'); do
	echo "$conf=${!conf}"
done | xargs | tr ' ' ':')

# fetch ID and SGF mappings
declare -A SGF
while IFS= read -r res; do
	res=$(xargs <<< ${res%%#*} | tr ' ' '\t')
	[[ $res ]] && SGF[$(cut -f1 <<< $res)]=$(cut -f2- <<< $res)
done <<< $(<${OPENING_FILE:-.opening.txt})

# check if arg 1 is game_type
if [[ ! -v SGF[$1] ]] && [ -d build/$1 ]; then
	GAME=$1
	shift
fi

# unalias ID groups
IDs=$(printf "%s\n" $IDs "$@" | grep -v = | xargs | tr ':' ' ')
while grep -q '{' <<< $IDs; do
	for alias in $(xargs -n1 <<< $IDs | grep '{'); do
		if [[ -v SGF[$alias] ]]; then
			IDs=${IDs/$alias/${SGF[$alias]}}
		else
			echo "$alias not found!"
			exit 100
		fi
	done
done
# check if IDs are available
IDs=($IDs)
if [[ ${IDs[@]} ]]; then
	for ID in ${IDs[@]}; do
		if [[ ! -v SGF[$ID] ]]; then
			echo "$ID not found!"
			exit 100
		fi
	done
else
	usage
	exit 1
fi

chat() {
	local chat pid
	exec {chat}<>/dev/tcp/${CHAT/://} || exit $?
	cat <&$chat >&1 &
	pid=$!
	cat $@ | tee /dev/stderr >&$chat
	sleep 0.5
	kill $!
	return 0
}

# check online services
broker_state=$(chat <<< "$broker_name << query state" 2>&1)
if (( $? )); then
	echo "chat service unavailable!"
	exit 1
fi
if [[ $broker_state == *"failed chat"* || $broker_state != *"idle"* ]]; then
	echo "$broker_name unavailable!"
	exit 1
fi
ONLINE=$use_online_fine_tuning
TRAINER_LINK=${broker_name/b/${TRAINER_LINK_PREFIX:-LL}}
TRAINER_HOST=${broker_name/b/${HOST_PREFIX:-$(hostname | sed -E "s/[0-9].+$//")}}
if [[ $ONLINE == true ]] && [[ $(printf "%s << \n" ${TRAINER_LINK} ${TRAINER_LINK}-sp ${TRAINER_LINK}-op | chat 2>&1) == *"failed chat"* ]]; then
	echo "online service unavailable!"
	exit 1
fi
# check executable files and config files
[[ $RUN ]] || RUN=${GAME:-game}_solver
[[ $TRAINER_RUN ]] || TRAINER_RUN=$RUN
if [ -x build/${RUN%%_solver*}/$RUN ]; then
	RUN=build/${RUN%%_solver*}/$RUN
elif [ ! -x $RUN ]; then
	echo "$RUN not found!"
	exit 1
fi
if [ -x build/${TRAINER_RUN%%_solver*}/$TRAINER_RUN ]; then
	TRAINER_RUN=build/${TRAINER_RUN%%_solver*}/$TRAINER_RUN
elif [[ $ONLINE == true ]] && [ ! -x $TRAINER_RUN ]; then
	echo "$TRAINER_RUN not found!"
	exit 1
fi
[[ $GAME ]] || GAME=$(basename ${RUN%%_solver*})
if [ ! -e $CFG ]; then
	echo "$CFG not found!"
	exit 1
fi
if [[ $ONLINE == true ]]; then
	if [ ! -e $TRAINER_CFG ]; then
		echo "$TRAINER_CFG not found!"
		exit 1
	fi
	TRAINER_PORT=${TRAINER_PORT:-$({ grep -m1 ^zero_server_port= $TRAINER_CFG || echo 9999; } | sed -E "s/[^0-9]/ /g" | xargs -n1 | head -n1)}
	TRAINER_GPU=${TRAINER_GPU:-3}
fi
# check nn model file
if [ ! -d $NN_HOME ]; then
	echo "$NN_HOME not found!"
	exit 1
fi
if [[ $MODEL == *:* ]]; then
	nn_file_name=$NN_HOME/${MODEL%:*}/model/weight_iter_${MODEL#*:}.pt
elif [[ $MODEL && $(ls $NN_HOME/$MODEL/model/weight_iter_*.pt | sort -V | tail -n1) =~ weight_iter_(.+).pt ]]; then
	MODEL=$MODEL:${BASH_REMATCH[1]}
	nn_file_name=$NN_HOME/${MODEL%:*}/model/weight_iter_${MODEL#*:}.pt
elif [[ $nn_file_name =~ ^.+/(.+)/model/weight_iter_(.+).pt$ ]]; then
	MODEL=${BASH_REMATCH[1]}:${BASH_REMATCH[2]}
fi
if [ ! -e $nn_file_name ]; then
	echo "$nn_file_name not found!"
	exit 1
fi

# build config label
[[ $ONLINE != true ]] && solver_mode=baseline || solver_mode=online
label_env+=(${MODEL/:/-})
label_env+=($(sed -E "s|(.+/)?[^/]+_solver_?||g" <<< $RUN))
label_env+=($({ chat <<< "$broker_name << query solver_env" 2>&1 | grep '# solver_env' || echo $(<<< $broker_state grep ">> state" | cut -d/ -f2 | cut -d' ' -f1)w; } | cut -d= -f2))
label_env=${label_env[@]}
label_env=${label_env// /-}

# launch manager for each ID
for ID in ${IDs[@]}; do
	sgf=${SGF[$ID]%% *}	
	label=${TAG:+${TAG}-}${ID}-${solver_mode}-${actor_num_simulation}-${manager_pcn_value_threshold}-${label_env}${INFO:+-${INFO}}
	echo -n "$label ... "
	
	manager_conf_str="manager_job_sgf=$sgf:tree_file_name=$label:nn_file_name=$nn_file_name:actor_num_simulation=$actor_num_simulation:manager_pcn_value_threshold=$manager_pcn_value_threshold:use_online_fine_tuning=$use_online_fine_tuning:${FLAGS:+${FLAGS}:}broker_host=${CHAT%:*}:broker_port=${CHAT#*:}:broker_name=$broker_name:broker_adapter_name=$label:broker_logging=true"
	manager_conf_str=$(<<< $manager_conf_str tr ':' '\n' | awk '!x[$0]++' | xargs | tr ' ' ':')

	trainer_repo=$NN_HOME/online_$label
	trainer_conf_str="trainer_opening_sgf=$sgf:${TRAINER_FLAGS:+${TRAINER_FLAGS}:}zero_server_port=$TRAINER_PORT:broker_host=${CHAT%:*}:broker_port=${CHAT#*:}:broker_name=$broker_name"
	trainer_cmd="scripts/game-solver-trainer.sh $GAME $TRAINER_CFG $trainer_repo $NN_HOME/${MODEL/:/ } --sp_executable_file $TRAINER_RUN --conf_str \"$trainer_conf_str\""
	trainer_sp_cmd="scripts/zero-worker.sh $GAME $TRAINER_HOST $TRAINER_PORT sp -g $TRAINER_GPU -b 256 --sp_executable_file $TRAINER_RUN"
	trainer_op_cmd="scripts/zero-worker.sh $GAME $TRAINER_HOST $TRAINER_PORT op -g $TRAINER_GPU --sp_executable_file $TRAINER_RUN"

	# check cases that don't need to launch
	if [[ $DRYRUN == true ]]; then # print command and exit
		echo
		echo "$ $RUN -conf_file $CFG -mode manager -conf_str \"$manager_conf_str\""
		if [[ $ONLINE == true ]]; then
			echo "$ $trainer_cmd"
			echo "$ $trainer_sp_cmd"
			echo "$ $trainer_op_cmd"
		fi
		continue
		
	elif [[ $OVERWRITE != true ]] && ( [ -e $label.log ] || ( [[ $ONLINE == true ]] && [ -e $trainer_repo ] ) ); then # logfile/repo already exists
		echo "already exists!"
		continue
	elif [[ $ONLINE == true ]] && [ -e $trainer_repo ]; then # overwrite online model folder
		trainer_cmd="echo R | $trainer_cmd"
	fi

	{ # initialize online service sessions
		echo "$broker_name << set id_next 1"
		for worker in $(<<< $broker_state grep ">> state" | grep -Eo "\(.+\)" | tr -d "()[]" | xargs -n1 | cut -d= -f1); do
			echo "$worker << solver load_model $nn_file_name"
		done
		if [[ $ONLINE == true ]]; then
			echo "${TRAINER_LINK} << set $trainer_cmd"
			echo "${TRAINER_LINK}-sp << set sleep 10; $trainer_sp_cmd"
			echo "${TRAINER_LINK}-op << set sleep 10; $trainer_op_cmd"
			echo "${TRAINER_LINK}* << start"
			sleep 10
		fi
	} | chat >/dev/null 2>&1
	[[ $ONLINE == true ]] && trap 'echo "${TRAINER_LINK}* << stop" | chat >/dev/null 2>&1' EXIT
	
	# setup timeout watchdog
	unset watchdog_pid
	if (( ${TIMEOUT:-0} )); then
		{	sleep $TIMEOUT
			echo "$label << solver quit" | chat
		} >/dev/null 2>&1 &
		watchdog_pid=$!
	fi

	if [[ $GDB != true ]]; then # launch normally
		{ time \
			$RUN -conf_file $CFG -mode manager -conf_str "$manager_conf_str"
		} >$label.log 2>&1
	else # launch with GDB
		echo
		{ time gdb -ex='set confirm on' -ex='run' -ex='quit' --args \
			$RUN -conf_file $CFG -mode manager -conf_str "$manager_conf_str" 
		} 2>&1 | tee $label.log
		echo -en "time\t"
	fi

	{ # cleanup
		[[ $watchdog_pid ]] && kill $watchdog_pid $(pgrep -P $watchdog_pid)
		[[ $ONLINE == true ]] && echo "${TRAINER_LINK}* << stop" | chat
		trap - EXIT
		wait
	} >/dev/null 2>&1

	# generate statistics and display results
	scripts/parse-manager-results.sh stat $label >/dev/null 2>&1 &
	time=$(tac $label.log | egrep -m1 "^time:" | cut -d: -f2 | xargs)
	if [[ ! $time ]]; then
		time=$(tail $label.log | grep real | cut -f2)
		time=${time%s}; time=$(bc <<< "${time/m/*61+}")
	fi
	echo ${time}s
done
wait
exit 0
