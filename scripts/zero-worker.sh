#!/bin/bash
set -e

if [ $# -lt 4 ]
then
	./minizero/scripts/zero-worker.sh --help
else
	GAME_TYPE=$1
	HOST=$2
	PORT=$3
	TYPE=$4
	shift 4
	./minizero/scripts/zero-worker.sh ${GAME_TYPE} ${HOST} ${PORT} ${TYPE} --sp_executable_file build/${GAME_TYPE}/${GAME_TYPE}_solver --op_executable_file game_solver/trainer/learner/train.py $@
fi