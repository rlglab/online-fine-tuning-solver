#!/bin/bash
if [[ ! $@ ]]; then
	echo "Usage: $0 [STAT]..."
	echo
	echo "For each given .stat file, extract and display important fields using format"
	echo "stat: nodes time count jobs avg_time avg_nodes load_model solved_rate avg_loading"
	exit
fi
for stat in "$@"; do
	echo $stat: $(($(cut -d' ' -f3,7 < $stat | tr ' ' '+'))) $(cut -d' ' -f2,3,4,6,8,18,19 < $stat) $(bc -l <<< $(cut -d' ' -f24,25 < $stat | tr ' ' '/')*100 | xargs printf '%.2f%%')
done
