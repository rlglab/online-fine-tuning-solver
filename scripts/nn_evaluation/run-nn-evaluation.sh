#!/bin/bash

if [ $# -ne 6 ]; then
    echo "usage: ./run-nn-evaluation.sh network_dir record_dir start_iter end_iter benchmark config"
    exit
fi

solver="/workspace/Release/game_solver"
mode="worker"
formatter="/workspace/scripts/nn_evaluation/eval_result_format.py"
network_directory="/workspace/training/${1}/model"
record_directory=$2
start_iter=$3
end_iter=$4
benchmark=$5
conf_file=$6
step=5000

mkdir $record_directory

for ((i=start_iter; i<=end_iter; i=$((i + step))))
do
    echo "Evaluation with weight iter: ${i}"
    echo "Run: ${solver} -mode ${mode} -conf_file ${conf_file} -conf_str \"nn_file_name=${network_directory}/weight_iter_${i}.pt\" < ${benchmark} 1> ${record_directory}/temp.txt 2> /dev/null"
    $solver -mode $mode -conf_file $conf_file -conf_str "nn_file_name=${network_directory}/weight_iter_${i}.pt" < $benchmark 1> ${record_directory}/temp.txt 2> /dev/null
    echo "Evaluation result: eval_result_${i}.txt"
    ${formatter} 0 < ${record_directory}/temp.txt > ${record_directory}/eval_result_${i}.txt
    rm ${record_directory}/temp.txt
done

echo "Finish"
