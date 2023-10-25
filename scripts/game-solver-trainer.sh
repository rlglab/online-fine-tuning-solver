#!/bin/bash
set -e

usage()
{
	echo "Usage: ./game-solver-trainer.sh game_type configure_file train_dir anchor_train_dir anchor_model_iteration [OPTION...]"
	echo ""
	echo "  -h, --help                 Give this help list"
	echo "    , --sp_executable_file   Assign the path for self-play executable file"
	echo "    , --op_executable_file   Assign the path for optimization executable file"
	echo "    , --conf_str             Modify configure by string"
	exit 1
}

# check argument
if [ $# -lt 5 ]; then
	usage
else
	GAME_TYPE=$1
	CONFIGURE_FILE=$2
	TRAIN_DIR=$3
	ANCHOR_TRAIN_DIR=$4
	ANCHOR_MODEL_ITERATION=$5
	shift 5
fi

sp_executable_file=build/${GAME_TYPE}/${GAME_TYPE}_solver
op_executable_file=game_solver/trainer/learner/train.py
CONF_STR=""
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		--sp_executable_file) shift; sp_executable_file=$1
		;;
		--op_executable_file) shift; op_executable_file=$1
		;;
		--conf_str) shift; CONF_STR=$1
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
		;;
	esac
	shift
done

run_stage="R"
if [ -d ${TRAIN_DIR} ]; then
	echo "${TRAIN_DIR} has existed: Restart(R) / Quit(Q)."
	read -n1 run_stage
	echo ""
	if [[ ${run_stage} == "R" ]]; then
		rm -rf ${TRAIN_DIR}
	else
		exit
	fi
fi

# create train folder
echo "create ${TRAIN_DIR} ..."
mkdir ${TRAIN_DIR}
mkdir ${TRAIN_DIR}/model
mkdir ${TRAIN_DIR}/sgf
touch ${TRAIN_DIR}/op.log
NEW_CONFIGURE_FILE=$(echo ${TRAIN_DIR} | awk -F "/" '{ print ($NF==""? $(NF-1): $NF)".cfg"; }')
cp ${CONFIGURE_FILE} ${TRAIN_DIR}/${NEW_CONFIGURE_FILE}
cp ${ANCHOR_TRAIN_DIR}/model/weight_iter_${ANCHOR_MODEL_ITERATION}.* ${TRAIN_DIR}/model/
MODEL_FILE=${TRAIN_DIR}/model/weight_iter_${ANCHOR_MODEL_ITERATION}.pt

# check arguments
if [[ $(grep "^use_broker=true" ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} | wc -l) -eq 0 ]]; then
	echo "use_broker should be true when using game-solver-trainer.sh"
	exit
fi

# modify configuration by conf_str
for conf in $(echo ${CONF_STR} | sed 's/:/\n/g')
do
	key=$(echo ${conf} | cut -d '=' -f 1)
	sed -i "s/^${key}=.*/${conf}/g" ${TRAIN_DIR}/${NEW_CONFIGURE_FILE}
done

# run trainer
CONF_STR="zero_training_directory=${TRAIN_DIR}:zero_end_iteration=1000000:nn_file_name=${MODEL_FILE}:zero_start_iteration=1"
BROKER_HOST=$(grep "^broker_host=" ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} | cut -d '=' -f 2)
BROKER_PORT=$(grep "^broker_port=" ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} | cut -d '=' -f 2)
exec {chat_tcp}<>/dev/tcp/${BROKER_HOST}/${BROKER_PORT} || exit 1
${sp_executable_file} -conf_file ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} -conf_str ${CONF_STR} -mode zero_server 0<&$chat_tcp 1>&$chat_tcp {chat_tcp}>&-