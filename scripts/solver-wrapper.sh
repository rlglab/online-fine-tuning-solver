#!/bin/bash
# chat::node plugin for game solver
# set config "plugins=scripts/solver-wrapper.sh" to use this plugin

logs() { log "[solver] $@"; }

if [[ ${mode:-$(basename "$0" .sh)} == worker ]]; then
# ================ worker plugin begin ================
logs "load wrapper plugin for worker"

override execute_request
execute_request() {
	local id=$1
	if [[ ${cmd[$id]} == "solve "* ]]; then
		# check whether the solver is available
		check_solver_pid || launch_solver || return $?

		# input the question
		input_solver "+ $id ${cmd[$id]:6}" || return $?
		pid[$id]=${pid[solver]}
		return 0
	fi
	# execute the request by default procedure
	invoke_overridden execute_request "$@"
}

override kill_request
kill_request() {
	local id=$1
	if [[ -v pid[$id] ]] && [[ ${cmd[$id]} == "solve "* ]]; then
		if check_solver_pid && input_solver "- $id"; then
			:
		else
			logs "failed to kill $id, already killed?"
		fi
		unset pid[$id]
		return 0
	fi
	# execute the kill_request by default procedure
	invoke_overridden kill_request "$@"
}

launch_solver() {
	# setup folder for supporting files
	mkdir -p $(dirname $solver_run) 2>/dev/null || mkdir -p $(readlink -f $(dirname $solver_run)) 2>/dev/null || {
		logs "failed to setup run folder"; return 2; }

	# kill the existing solver when necessary
	kill_solver

	# prepare stdin and stdout pipes
	exec {stdin[solver]}<> <(:) || {
		unset stdin[solver]; logs "failed to initialize stdin pipe"; return 4; }
	exec {stdout[solver]}<> <(:) || {
		unset stdout[solver]; logs "failed to initialize stdout pipe"; return 4; }

	# launch the solver using a wrapper in the background
	logs "launch ${solver[@]}"
	{
		export_solver_env
		tee $solver_run.stdin | stdbuf -o0 "${solver[@]}" | tee $solver_run.stdout | parse_solver_output
	} 0<&${stdin[solver]} {stdin[solver]}<&- 1>&${stdout[solver]} {stdout[solver]}>&- 2>$solver_run.stderr &
	local wrapper_pid=$!

	# locate the PID of the launched solver instance
	local tick
	while [[ ! ${pid[solver]} ]] && ps -p $wrapper_pid >/dev/null && (( tick++ < ${solver_launch_timeout:-60} )) && sleep 1; do
		local solver_pid
		for solver_pid in $(pgrep -P $wrapper_pid); do
			local locate_cmd=$(xargs -0 </proc/$solver_pid/cmdline)
			[[ $locate_cmd == ${solver[@]} ]] && pid[solver]=$solver_pid
		done
	done
	
	# check launch status
	if check_solver_pid; then
		logs "launched solver at ${pid[solver]} successfully"
		return 0
	else
		logs "failed to launch solver"
		kill $wrapper_pid 2>/dev/null
		kill_solver
		return 1
	fi
}

export_solver_env() {
	export CUDA_VISIBLE_DEVICES=${CUDA_VISIBLE_DEVICES:-$((${name##*-} % $(nvidia-smi -L | wc -l)))}
}

parse_solver_output() {
	local solver_out=
	local solver_res=()
	while IFS= read -r solver_out; do
		if [[ $solver_out ]]; then
			solver_res+=("$solver_out")
		elif [[ ${solver_res[@]} ]]; then
			solver_out=$(echo -n "$(printf "%s\n" "${solver_res[@]}")" | format_output)
			echo "$solver_out"
			solver_res=()
		fi
	done
}

input_solver() { { echo "$@" >&${stdin[solver]}; } 2>/dev/null; }
output_solver() { read -r -t 0 -u ${stdout[solver]} && IFS= read -r -u ${stdout[solver]} $1; }
load_model_solver() { [ -e "$1" ] && input_solver "load_model $1"; }

check_solver_pid() { [[ ${pid[solver]} ]] && ps -p ${pid[solver]} >/dev/null 2>&1; }
check_solver_cmd() { [[ ${pid[solver]} && $(fetch_solver_cmd) == ${solver[@]} ]]; }
fetch_solver_cmd() { { xargs -0 </proc/${pid[solver]:-x}/cmdline; } 2>/dev/null; }

override fetch_response
fetch_response() {
	local response
	if [[ ${stdout[solver]} ]] && output_solver response; then
		id=${response%% *}
		code=0
		output=${response#* }
		if [[ $id != *[^0-9]* ]]; then
			return 0
		elif [[ $id == num_solvers ]]; then
			set_affinity $output
		else
			logs "ignore solver output $response"
		fi
		return 1
	else
		invoke_overridden fetch_response "$@"
	fi
}

kill_solver() {
	[[ -v registered ]] || return 1
	# kill the running solver process
	if [[ ${pid[solver]} ]]; then
		logs "kill $(fetch_solver_cmd || echo "N/A")"
		kill ${pid[solver]} 2>/dev/null
	fi
	# close stdin/stdout pipes
	[[ -v stdin[solver] ]]  && exec {stdin[solver]}<&-
	[[ -v stdout[solver] ]] && exec {stdout[solver]}>&-
	# release variables and other resources
	archive_old_solver_files
	flush_unfinished_solver_requests
	unset pid[solver] stdin[solver] stdout[solver]
	[[ ${affinity:-1} != 1 ]] && set_affinity 1
	return 0
}

archive_old_solver_files() {
	if ls $solver_run.{stdin,stdout,stderr} >/dev/null 2>&1; then
		local archive="solver-wrapper.archive/$(date '+%Y%m%d-%H%M%S')-$(name)"
		mkdir -p "$archive" && mv -f $solver_run.{stdin,stdout,stderr} "$archive/" 2>/dev/null
		logs "cleanup, move $solver_run related files into $archive/"
	fi
}

flush_unfinished_solver_requests() {
	local id
	for id in ${!pid[@]}; do
		if [[ ${cmd[$id]} == "solve "* ]]; then
			logs "unfinished request $id, response with code 127"
			{ echo "response $id 127 {}" >&$res_fd; } 2>/dev/null
		fi
	done
}

override handle_noinput
handle_noinput() {
	if check_solver_pid; then
		:
	elif [[ ${pid[solver]} ]]; then
		kill_solver
	fi
	invoke_overridden handle_noinput
}

override init_configs
init_configs() {
	invoke_overridden init_configs "$@"
	declare -g buffered_input=true
	
	declare -g game_type=${game_type:-${game:-killallgo}}
	declare -g solver=${solver}
	[[ $solver ]] || solver="build/${game_type}/${game_type}_solver -conf_file worker.cfg -mode worker"
	eval solver=("$solver")

	declare -g solver_run=$(mktemp -u -p "solver-wrapper.run" $(hostname)-XXXX)

	contains configs broker || contains configs brokers || \
		brokers=($(grep broker_name ${conf_file:-worker.cfg} 2>/dev/null | cut -d= -f2))

	contains configs system_tick || system_tick=0.01
	contains configs capacity || capacity=
	contains configs affinity || affinity=1
}

override set_config
set_config() {
	if invoke_overridden set_config "$@"; then
		if [[ $1 == solver ]]; then
			eval solver=("$solver")
		elif [[ $1 == solver_cmd ]]; then
			erase_from configs solver_cmd
			contains configs solver || configs+=(solver)
			eval solver=("$solver_cmd")
		fi
		return 0
	fi
	return $?
}

override unset_config
unset_config() {
	if invoke_overridden unset_config "$@"; then
		if [[ $1 == solver || $1 == solver_cmd ]]; then
			erase_from configs solver solver_cmd
			local game_type=${game_type:-${game:-killallgo}}
			solver=(build/${game_type}/${game_type}_solver -conf_file worker.cfg -mode worker)
		fi
		return 0
	fi
	return $?
}

override cleanup
cleanup() {
	kill_solver
	invoke_overridden cleanup "$@"
}

override prepare_shutdown
prepare_shutdown() {
	kill_solver
	invoke_overridden prepare_shutdown "$@"
}

override prepare_restart
prepare_restart() {
	kill_solver
	invoke_overridden prepare_restart "$@"
}

handle_solver_input() { # ^solver (.+)$
	local option=${1:7}
	local who=${2}

	if [[ $option == status* || $option == state* ]]; then
		local status=()
		status+=("cmd = $(fetch_solver_cmd || echo "N/A")")
		status+=("pid = ${pid[solver]:-"N/A"}")

		logs "accept solver $option from $who"
		[[ $option = *forward* ]] && who=${option#*forward }
		printf "%s\n" "${status[@]}" | xargs_eval logs "status:"

		if check_solver_pid && check_solver_cmd; then
			echo "$who << confirm solver status OK"
			printf "$who << # %s\n" "${status[@]}"
		elif [[ -v pid[solver] ]]; then
			echo "$who << confirm solver status BAD"
			printf "$who << # %s\n" "${status[@]}"
		else
			echo "$who << confirm solver status N/A"
		fi

	elif [[ $option == input* || $option == load_model* ]]; then
		local input=${option#* }
		if check_solver_pid; then
			echo "$who << confirm solver ${option%% *}"
			if ${option%% *}_solver "${option#* }"; then
				logs "accept solver ${option} from $who"
			else
				logs "failed to execute solver ${option} from $who"
			fi
		else
			logs "ignore solver ${option} from $who"
		fi

	elif [[ $option == launch || $option == kill ]]; then
		logs "execute solver $option from $who"
		if ${option%% *}_solver "${option#* }"; then
			echo "$who << confirm solver ${option%% *}"
		else
			logs "failed to ${option%% *} solver"
		fi

	elif [[ $option == binary* ]]; then
		logs "execute solver $option from $who"
		if [[ $option =~ ^binary\ +(.+)$ ]]; then
			local binary=${BASH_REMATCH[1]}
			[ -e "$binary" ] || binary=$(find -L build -name $binary -type f -executable | head -n1)
			echo "$who << confirm solver ${option%% *}"
			solver[0]=$binary
		else
			logs "ignore solver ${option} from $who"
		fi

	elif [[ $option == cmdline* ]]; then
		logs "execute solver $option from $who"
		if [[ $option =~ ^cmdline\ +(.+)$ ]]; then
			echo "$who << confirm solver ${option%% *}"
			set_config solver "${BASH_REMATCH[1]}"
		else
			logs "ignore solver ${option} from $who"
		fi

	else
		logs "ignore unknown solver $option from $who"
		return 1
	fi

	return 0
}

# ================ worker plugin end ==================

elif [[ ${mode:-$(basename "$0" .sh)} == broker ]]; then
# ================ broker plugin begin ================
logs "load wrapper plugin for broker"

override init_configs
init_configs() {
	invoke_overridden init_configs "$@"
	declare -g buffered_input=true
	
	declare -g trainer

	contains configs system_tick || system_tick=0.01
	contains configs capacity || capacity=+65536
	contains configs affinity || affinity=0
}

override node_logout
node_logout() {
	local who=$1
	invoke_overridden node_logout $who
	if [[ $trainer == $who ]]; then
		logs "trainer $who logged out"
		trainer=
	fi
}

override node_rename
node_rename() {
	local who=$1 new=$2
	invoke_overridden node_rename $who $new
	if [[ $trainer == $who ]]; then
		logs "trainer $who renamed as $new"
		trainer=$new
	fi
}

handle_solver_input() { # ^solver (.+)$
	local option=${1:7}
	local who=${2}

	if [[ $option == openings* || $option == solved_position* || $option == solved_sgf* ]]; then
		#echo "$who << confirm solver ${option%% *}"
		if [[ $trainer && $who != $name ]]; then
			echo "$trainer << solver $option"
			logs "forward solver ${option%% *} to $trainer"
		else
			logs "ignore solver ${option%% *} from $who"
		fi

	elif [[ $option == load_model* ]]; then
		local clients=()
		local load_model=${solver_load_model:-manager+workers}
		[[ $load_model == *worker* ]] && clients+=(${!state[@]})
		[[ $load_model == *manager* ]] && clients+=($(printf "%s\n" ${own[@]} | sort -u))
		if [[ ${clients[@]} ]]; then
			printf "%s << solver $option\n" ${clients[@]}
			echo "$who << confirm solver ${option%% *}"
			logs "forward solver ${option%% *} to ${clients[@]}"
		else
			logs "ignore solver ${option%% *} from $who"
		fi

	elif [[ $option == status || $option == state ]]; then
		if [[ ${!state[@]} ]]; then
			echo "$who << confirm solver ${option%% *}"
			local worker
			for worker in $(printf "%s\n" ${!state[@]} | sort); do
				echo "$worker << solver $option forward $who"
				sleep 0.5
			done &
			logs "forward solver ${option%% *} to ${!state[@]}"
		else
			logs "ignore solver ${option%% *} from $who"
		fi

	elif [[ $option == launch || $option == kill || $option == cmdline* || $option == binary* ]]; then
		if [[ ${!state[@]} ]]; then
			printf "%s << solver $option\n" ${!state[@]}
			echo "$who << confirm solver ${option%% *}"
			logs "forward solver ${option%% *} to ${!state[@]}"
		else
			logs "ignore solver ${option%% *} from $who"
		fi

	elif [[ $option == "register trainer" ]]; then
		logs "confirm that $who is a trainer"
		[[ $trainer ]] && logs "warning! the previous trainer $trainer is still online"
		trainer=$who

	else
		logs "ignore unknown solver $option from $who"
		return 2
	fi

	return 0
}

# ================ broker plugin end ==================

fi

override handle_confirm_input
handle_confirm_input() {
	invoke_overridden handle_confirm_input "$@" && return 0
	local confirm=${1%% *}
	local details=${1#* }
	local who=${2}
	if [[ $details == solver* ]]; then
		logs "confirm that $who ${confirm}ed $details"
		return 0
	fi
	return 1
}

override extract_options
extract_options() { [[ ! $options ]]; }

override optionalize_request
optionalize_request() { :; }
