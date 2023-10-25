#!/bin/bash
if [[ $1 == stat ]]; then
	label=${2%.log}
	if [[ $label ]]; then
		exec 0<$label.log
		# exec 1>$label.stat
	fi

	tmpdir=$(mktemp -d)
	trap 'rm -r $tmpdir' EXIT
	name=${label%.log}; name=${name:-unknown}
	cat > $tmpdir/$name.log
	$0 jobs $tmpdir/$name &
	pushd $tmpdir >/dev/null
	count_label() { grep -F "$@" $name.log | wc -l; }
	nstat="$(count_label 'already solved') $(count_label 'unsolved!') $(count_label 'black wins!') $(count_label 'is_root_virtual_solved_1') $(count_label 'is_root_virtual_solved_0') $(count_label 'load_model')"
	wait

	njobs=$(wc -l < $name.jobs)
	#ujobs=$(cut -d' ' -f2,3 < $name.jobs | sort | uniq | wc -l)
	tjobs=$(cut -d' ' -f8 < $name.jobs | tr -d "ms" | xargs | tr ' ' '+' | bc | xargs | tr ' ' '+' | bc)
	#ajobs=$((tjobs/$njobs))
	tjobs=${tjobs:0: -3}.${tjobs: -3}
	#ajobs=${ajobs:0: -3}.${ajobs: -3}
	sjobs=$(cut -d' ' -f6 < $name.jobs | xargs | tr ' ' '+' | bc | xargs | tr ' ' '+' | bc)
	time=$(tac $name.log | egrep -m1 "^time:" | cut -b7-)
	if [[ ! $time ]]; then
		time=$(tail $name.log | grep real | cut -f2)
		time=${time%s}; time=$(bc <<< "${time/m/*61+}")
	fi
	jstats=($(
	{
		echo 1; echo; echo -1; echo -2;
		cut -d' ' -f4 < $name.jobs
	} | sort -rn | uniq -c | sed -E "s/^ +//g" | cut -d' ' -f1
	))
	jstat="$((jstats[0]-1)) $((jstats[2]-1)) $((jstats[3]-1)) $((jstats[1]-1))" # 1 -1 -2 n/a
	load=()
	avg=0 lim=0
	while read -r count total; do
		i=$(((total-1)/32))
		load[$i]=$((load[$i]+count))
		avg=$((avg+total*count))
		lim=$total
	done <<< $(<$name.log grep state | grep confirm | cut -d' ' -f7 | cut -d'/' -f1 | sort -n | uniq -c)
	avg=$(printf "%.2f" $(echo "$avg/(${load[@]})" | tr ' ' '+' | bc -l))
	echo ${time}s ${njobs}jobs: ${tjobs}s ${sjobs} [$jstat] {${nstat}} $avg/$lim: [${load[@]}] > $name.stat
	# echo ${time}s ${tjobs}s ${njobs}jobs ${ujobs}jobs $jstat

	popd >/dev/null
	if [[ $label ]]; then
		mv $tmpdir/$name.stat $label.stat
		mv $tmpdir/$name.jobs $label.jobs
		$0 show $label >/dev/null
	else
		$0 show $tmpdir/$name
	fi
	exit 0

elif [[ $1 == jobs ]]; then
	label=${2%.log}
	if [[ $label ]]; then
		exec 0<$label.log
		exec 1>$label.jobs
	fi

	repo=$(mktemp -d)
	trap 'rm -r $repo' EXIT
	cat > $repo/log
	pushd $repo >/dev/null

	grep -F "confirm accepted request" log | grep -v '{}$' | sed -e 's/confirm accepted request //g' -e 's/{solve "//g' -e 's/"}$//g' > req &
	grep -F " accept response" log | sed -e 's/accept response //g' -e 's/0 {//g' -e 's/}$//g' > res &
	wait

	normalize() {
		cut -d' ' -f3 $1 > $1.ID &
		cut -d' ' -f4- $1 > $1.data &
		{
		cut -b1-19 $1 | xargs -I{} date +%s -d "{}" > $1.sec &
		cut -b21-23 $1 > $1.ms &
		wait
		paste $1.sec $1.ms | tr -d '\t' > $1.when
		} &
		wait
		paste $1.ID $1.when $1.data | sort -n > $1
	}
	normalize req &
	normalize res &
	wait

	{
	sed "s/^/req\t/g" < req
	sed "s/^/res\t/g" < res
	} | sort -k2 -ns > jobs

	pass=
	while IFS= read -r line; do
		if [[ $line == req* ]]; then
			$pass
			IFS=$'\t' read -r what id last req <<< $line && echo -n "$id $req"
		else
			IFS=$'\t' read -r what id this res <<< $line && echo -n " $res $((this-last))ms"
		fi
		pass=echo
	done < jobs
	$pass

	popd >/dev/null
	exit 0

elif [[ $1 == show && $2 ]]; then
	label=${2}
	label=${label%.stat}
	label=${label%.sgf}
	label=${label%.log}
	[ -e $label.stat ] || exit $?
	if grep -q '{' $label.stat; then
		{
		if [ -e $label.sgf ]; then
			status=$(head -n1 $label.sgf | grep -Eo "solver_status: \S+" | cut -d' ' -f2)
			status=${status,,}
			[[ $status ]] && echo -n "$status " || echo -n "? "
		else
			echo -n "? "
		fi
		if [ -e $label.stat ]; then
			res=$(<$label.stat)
			res=$(<<< $res sed -e "s/\// /g" -e "s/}/     /g" | sed -E "s/[^0-9. ]//g")
			stat="$(<<< $res cut -d' ' -f1)  $(<<< $res cut -d' ' -f2,3)  $(<<< $res cut -d' ' -f4-)"
			echo -n "${stat%% *}"
			if [ -e $label.sgf ]; then
				count=$(head -n8 $label.sgf | grep -Eo "count = [0-9]+" | cut -d' ' -f3)
				[[ $count ]] && echo -n " $count" || echo -n " ?"
			else
				echo -n " ?"
			fi
			stat=${stat#*  }
			num_jobs=${stat%% *}
			echo -n " $num_jobs"; stat=${stat#* }
			total_time=${stat%% *}
			echo -n " $total_time"; stat=${stat#* }
			avg_time=$(<<< $total_time/$num_jobs bc -l | grep -Eo "^([0-9]+)?.[0-9]{4}")
			echo -n " $avg_time"
			stat=${stat# }
			total_nodes=${stat%% *}
			echo -n " $total_nodes"; stat=${stat#* }
			avg_nodes=$(<<< $total_nodes/$num_jobs bc -l | grep -Eo "^([0-9]+)?.[0-9]{4}")
			echo -n " $avg_nodes"
			num_status=(${stat})
			stat=${num_status[@]:10}
			num_status=(${num_status[@]:0:10})
			sum_st=$((num_status[0]+num_status[1]+num_status[2]+num_status[3]))
			(( sum_st )) || sum_st=1
			echo -n " ${num_status[@]}"
			echo -n " $(<<< ${num_status[0]}*100/$sum_st bc -l | grep -Eo "^([0-9]+)?.[0-9]{0,2}")%"
			echo -n " $(<<< ${num_status[1]}*100/$sum_st bc -l | grep -Eo "^([0-9]+)?.[0-9]{0,2}")%"
			echo -n " $(<<< ${num_status[2]}*100/$sum_st bc -l | grep -Eo "^([0-9]+)?.[0-9]{0,2}")%"
			echo -n " $(<<< ${num_status[3]}*100/$sum_st bc -l | grep -Eo "^([0-9]+)?.[0-9]{0,2}")%"
			sum_vs=$((num_status[7]+num_status[8]))
			(( sum_vs )) || sum_vs=1
			echo -n " $(<<< ${num_status[7]}*100/$sum_vs bc -l | grep -Eo "^([0-9]+)?.[0-9]{0,2}")%"
			echo -n " ${stat}"
		else
			echo -n "unknown"
		fi
		echo
		} > $label.show
		mv $label.show $label.stat
	fi
	cat $label.stat
	exit 0

else
	echo "Usage: $0 [stat|jobs] [LABEL]"
fi
