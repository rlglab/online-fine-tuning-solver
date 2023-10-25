#!/bin/bash
set -e

if [ $# -lt 4 ]
then
	./minizero/scripts/zero-server.sh --help
else
	GAME_TYPE=$1
	CONFIGURE_FILE=$2
	TRAIN_DIR=$3
	END_ITERATION=$4
	shift 4
	./minizero/scripts/zero-server.sh ${GAME_TYPE} ${CONFIGURE_FILE} ${TRAIN_DIR} ${END_ITERATION} --sp_executable_file build/${GAME_TYPE}/${GAME_TYPE}_solver --op_executable_file game_solver/trainer/learner/train.py $@
fi