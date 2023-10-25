#!/bin/bash

if [[ ! $@ ]]; then
	cat <<- USAGE
	Usage: $0 [GAME|EXEC_FILE] [CONFIG] [MACHINE]... [-conf_file CFG_FILE] [-conf_str CFG_STR]
	Setup pre-defined solver environment for the baseline and the online fine-tuning solver.
	E.g.,  $0 killallgo 8B NV20 NV21
	       $0 killallgo 4B NV21 -conf_file worker.cfg

	Options:
	  GAME       game type, e.g., "killallgo" indicates launching build/killallgo/killallgo_solver
	  EXEC_FILE  executable file name, e.g., "killallgo_solver_abcdef" indicates launching build/killallgo/killallgo_solver_abcdef
	             the executable file should be given without folder, and must be named as {GAME_TYPE}_solver{ANY_SUFFIX}
	  CONFIG     solver configuration, supports 8B|8O|4B|4O, namely [8] or [4] GPUs; [B]aseline or [O]nline
	             other settings are not supported by this script
	  MACHINE    machines to run workers: 4B and 4O require one machine; 8B and 8O require two
	             the first specified machine will be set as the broker of the configured environment
	  CFG_FILE   configuration file for workers, default value "config/worker.cfg"
	  CFG_STR    configuration string for workers
	
	USAGE
	exit 0
fi

# parse game type or executable file
if [ -e build/${1}/${1}_solver ]; then # e.g., build/killallgo/killallgo_solver
	exec_file=build/${1}/${1}_solver
	shift
elif [ -e build/${1%%_solver*}/${1} ]; then # e.g., build/killallgo/killallgo_solver_abcdef
	exec_file=build/${1%%_solver*}/${1}
	shift
else
	exec_file=build/killallgo/killallgo_solver
fi
if [[ ! -x $exec_file ]]; then
	echo "$exec_file not found"
	exit 1
fi

# parse worker configuration
mode=${1:-none}
shift
[[ $mode == ?L ]] && mode=${mode/L/O}
declare -A num_workers=(
	[4B]=192
	[4O]=192
	[8B]=384
	[8O]=384
)
if [[ ! -v num_workers[$mode] ]]; then
	echo "invalid configuration: $mode"
	exit 1
fi

# parse machines and hostnames
if [[ $mode == 4* ]]; then
	b=($1)
elif [[ $mode == 8* ]]; then
	b=($1 $2)
fi
shift ${#b[@]}
[[ $mode == 8* && ! ${b[1]} ]] && exit 3
HOST_PREFIX=${HOST_PREFIX:-$(hostname | sed -E "s/[0-9].+$//")}
b=(${b[@]/${HOST_PREFIX}/b})
NV=(${b[@]/b/${HOST_PREFIX}})

# parse conf_file and conf_str
conf_file=${conf_file:-config/worker.cfg}
conf_str=${conf_str}
while (( $# )); do
	token=${1#-}
	shift
	if [[ $token == conf_file=* || $token == conf_str=* ]]; then
		declare "$token"
	elif [[ $token == conf_file || $token == conf_str ]]; then
		declare "$token=${1#-}"
		shift
	else
		conf_str+=${conf_str:+:}$token
	fi
done
if [[ ! -e $conf_file ]]; then
	echo "$conf_file not found"
	exit 1
fi

# calculate number of worker processes
num_worker_processes=${mode:0:1}
[[ $mode == *O* ]] && num_worker_processes=$((num_worker_processes-1))
actor_num_parallel_games=$((num_workers[$mode]/num_worker_processes))
if [[ $((actor_num_parallel_games*num_worker_processes)) != ${num_workers[$mode]} ]]; then
	actor_num_parallel_games=$((actor_num_parallel_games+1))
	actor_num_uniq_workers=$(for nv in ${NV[@]}; do printf "$nv-%s\n" 0 1 2 3; done)
	[[ $mode == *O* ]] && actor_num_uniq_workers=$(grep -v "^$NV-3$" <<< $actor_num_uniq_workers)
	actor_num_uniq_workers=($(sort -t- -k2n <<< $actor_num_uniq_workers | head -n$((actor_num_parallel_games*num_worker_processes-num_workers[$mode]))))
fi
# setup environment flag
env=${num_worker_processes}g${num_workers[$mode]}w
[[ $mode == *O* ]] && env+=+1sp1op

chat() { scripts/chat/chat.sh 2>&1 | grep -v "^#"; }

{ # prompt of setting configuration
printf "%s << query solver_env\n" "${b[@]}"
sleep 0.5
printf "%s << query state\n" "${b[@]}"
sleep 0.5
printf "%s-? << query state\n" "${NV[@]}"
sleep 0.5
} | chat | grep -Ev "<<|options|failed" | sort -i
read -p "continue $env configuration? " confirm

{ # kill existing processes and launch new processes
printf "%s << set id_next 1\n" ${b[@]}
printf "%s << solver kill\n" ${b[@]}
printf "%s-* << solver kill\n" ${NV[@]}
printf "%s << operate discard workers\n" ${b[@]}
sleep 0.5
printf "%s-* << unset broker\n" ${NV[@]}
sleep 0.5
printf "%s-* << set broker $b\n" ${NV[@]}
sleep 0.5
[[ $mode == *O* ]] && echo "$NV-3 << unset broker"
} | chat
sleep 5
while chat <<< "$b << query state" 2>&1 | xargs | grep -vq "0/$num_worker_processes"; do # wait until all processes are killed
	sleep 5
done
echo "$b << query state" | chat

{ # launch worker process instances
echo "$b << solver cmdline $exec_file -conf_file $conf_file -mode worker -conf_str \"actor_num_parallel_games=${actor_num_parallel_games}${conf_str:+:${conf_str}}\""
[[ ${actor_num_uniq_workers[@]} ]] && sleep 0.5 && for worker in ${actor_num_uniq_workers[@]}; do
	echo "$worker << solver cmdline $exec_file -conf_file $conf_file -mode worker -conf_str \"actor_num_parallel_games=$((actor_num_parallel_games-1))${conf_str:+:${conf_str}}\""
done

sleep 0.5
echo "$b << solver launch"
sleep 10
while chat <<< "$b << query state" 2>&1 | xargs | grep -vq "0/${num_workers[$mode]}"; do # wait until all processes are launched
	sleep 10
done
echo "$b << query state"
echo "$b << set solver_env $env"
} | chat
read -p "continue worker warm-up? " confirm

# worker initial forward
num_request=$((${num_workers[$mode]}*2))
{
for (( i=0; i<${num_request:-384}; i++ )); do
	echo "$b << request solve \"(;FF[4]CA[UTF-8]AP[GoGui:1.5.1]SZ[7]KM[6.5]DT[2022-05-20];B[dc];W[];B[de];W[df];B[ce];W[ee];B[ed];W[fe];B[fd])\""
done
sleep 0.5
echo "$b << set id_next 1"
sleep $((num_request/8))
echo "$b << query state"
} | chat

# done
echo "$b has been configured as $env"
