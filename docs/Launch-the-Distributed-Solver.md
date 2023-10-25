## Launch the Distributed Solver

The distributed solver has an architecture consisting of a manager, several workers, and a trainer, in which these components communicate using a broker over a chat service. This subsection introduces how to configure the distributed environment as well as launch the distributed solver.
* [Start the chat service](#Start-the-chat-service)
* [Deploy the environment](#Deploy-the-environment)
    * [Deploy the environment using pre-configured tmux session](#Deploy-the-environment-using-pre-configured-tmux-session)
    * [Deploy the environment maunally](#Deploy-the-environment-maunally)
* [Launch the solver system](#Launch-the-solver-system)
    * [Prepare the solver and the configuration files](#Prepare-the-solver-and-the-configuration-files)
    * [Prepare the configuration for the manager](#Prepare-the-configuration-for-the-manager)
    * [Prepare the configurations for the workers and the trainer](#Prepare-the-configurations-for-the-workers-and-the-trainer)
    * [Configure the workers using pre-configured script](#Configure-the-workers-using-pre-configured-script)
    * [Configure the workers manually](#Configure-the-workers-manually)
    * [Launch the manager (and the trainer)](#Launch-the-manager-and-the-trainer)
    * [Specify other configurations](#Specify-other-configurations)
    * [Handle solver results](#Handle-solver-results)

### Start the chat service

The chat service provides communication between computing nodes.

#### Check service availability

Use `nc` to check whether a chat service is running.
```bash
nc NV18 8888 # connect to the service at NV18:8888
```
A login message appears once you successfully connected to the chat service, e.g., `# login: u1`.

In most cases, you can use a running chat service directly no matter who launched it.
However, if there is no chat service available, follow below steps to launch it.

#### Build and launch the service

Compile the executable for the chat service.
```bash
# run scripts/start-container.sh to launch the container for compiling
cd chat && make
```

Start the chat service.
```bash
# should be launched using tmux
scripts/chat/start-service.sh 8888 # use port 8888
```
The script will output only a single line, showing the executed command, e.g.,
```
# this is the output of start-service.sh
./chat/chat 8888 2>&1 | gzip > chat-NV18-20230301.log.gz
```

You may use `nc` to check the service availability as described above.
To stop the service, simply use Ctrl+C. 

> **Warning**: Remember to stop the service if the service is launched by you. However, make sure no one else is using the service before stopping it.

If the script exits immediately, it means that the service failed to start (usually caused by a port that is already in use).

To check the log, e.g., `chat-NV18-20230301.log.gz`, decompress it first.
```bash
gzip -vd chat-NV18-20230301.log.gz # get chat-NV18-20230301.log
```

### Deploy the environment

The environment consists of a set of computing nodes connected to the same chat service.
For standard configuration, you may deploy the environment using the pre-configured tmux session. Otherwise, you need to deploy it manually.

#### Deploy the environment using pre-configured tmux session

The pre-configured session assumes that each machine is named as `%s%d` (e.g., NV20, RLG02) and has four available GPUs. For different hardware setups, you should deploy manually.

Run this script on every machine reserved for workers and trainers.
```bash
scripts/setup-solver-tmux-session.sh CHAT=NV18:8888
```

Once the session is successfully launched, it will be named `solver` and will have three windows, `broker`, `workers`, and `trainer`, covering all components required for the communication.

For most cases, you may simply detach this session (Ctrl+b, `d`). More specifically, you are NOT required to modify the launched tmux session for running standard configurations, i.e., switching between standard configurations does not require modifying the tmux session.

However, after completing all the experiments, you will eventually need to clean up the deployed environment to release resources. On each deployed machine, you should follow below steps.

1. Attach the tmux session using `tmux a -t solver` if the `solver` session is detached.
2. Run the script as follows and wait until it ends. You will see several `# logout: xx` messages, which means that the components are terminated successfully.
```bash
scripts/stop-solver-tmux-components.sh CHAT=NV18:8888
```
3. Terminate all the containers inside the session. There are four containers in the `workers` window, and three in the `trainer` window.
4. Kill the whole session (Ctrl+b, `:kill-session`). Do NOT kill the session directly until all containers have been terminated.

> **Warning**: Remember to clean up the environment after your experiments. Otherwise, the launched worker processes will stay there, occupying system resources.

#### Deploy the environment maunally

The environment need to be manually deployed if your hardware setup has a different GPU topology compare to the default setup. Also, if you do not know whether the pre-configured tmux session will work for your hardware setup, you should manually deploy.

The complete environment includes a broker node, several worker nodes, and three trainer nodes. Standard scripts are provided for deploying nodes. Follow the following steps to deploy them.

1. Launch a new tmux session: `tmux new -A -s solver`. It is recommended to use different tmux windows to start different types of nodes.

2. Broker nodes should be named with `b` prefixed and followed by an index number. 
To launch a broker node named `b1`, run
```bash
# do NOT run the broker node inside the container
CHAT=NV18:8888 scripts/chat/broker.sh b1
```

3. Worker nodes should be named with the machine hostname prefixed, and suffixed with `-x`, where `x` corresponds to the target GPU. 
To launch four worker nodes named `W1-0`, `W1-1`, `W1-2`, and `W1-3`, run
```bash
# always run worker nodes inside the container (scripts/start-container.sh)
# you MUST run four worker nodes in four different terminals (tmux panes)
CHAT=NV18:8888 scripts/chat/worker.sh W1-0
CHAT=NV18:8888 scripts/chat/worker.sh W1-1
CHAT=NV18:8888 scripts/chat/worker.sh W1-2
CHAT=NV18:8888 scripts/chat/worker.sh W1-3
```

4. Trainer nodes should be named with `LL` prefixed and followed by the same index of its broker. A valid trainer deployment consists of three nodes: `LLx`, `LLx-sp`, and `LLx-op`, where `x` is the index of the broker. 
To launch trainer nodes `LL1`, `LL1-sp`, and `LL1-op`, run
```bash
# always run trainer nodes inside the container (scripts/start-container.sh)
# you MUST run three trainer nodes in different terminals (tmux panes)
CHAT=NV18:8888 scripts/chat/trainer.sh LL1
CHAT=NV18:8888 scripts/chat/trainer.sh LL1-sp
CHAT=NV18:8888 scripts/chat/trainer.sh LL1-op
```

5. Finally, start a [nc terminal](#Check-service-availability) and run `who` to check whether all nodes are online correctly.

This example illustrates how to deploy nodes associated with four GPUs on a machine. You should modify the launched commands accordingly. For example, if you plan to use a baseline solver with workers on only GPU 0, you may run commands for `b1` and `W1-0`, then skip all other nodes. Furthermore, worker nodes can be deployed across machines, for example, you can launch `W2-0`, `W2-1`, ..., on another machine `W2`.

In addition, for an online fine-tuning solver, [`run-manager.sh`](#Launch-the-manager-and-the-trainer) (will be introduced in a later subsection) assumes that the trainer uses GPU 3. If your setup does not allow the trainer to be executed on GPU 3, set `TRAINER_GPU` to specify the GPU when starting `run-manager.sh`.



### Launch the solver system

For standard configurations (8g384w or 4g192w; baseline or online), you may configure the system using the pre-configured script. Otherwise, you have to configure it manually.

#### Prepare the solver and the configuration files

Build the solver executable using the provided build script as follows.
```bash
# run scripts/start-container.sh to launch the container for compiling
scripts/build.sh killallgo # build killallgo solver
```
In this tutorial, it is assumed that you are running 7x7 killall-go. For other games, you will have to modify the mentioned commands accordingly.

In addition, make sure that solver configuration files (`*.cfg`) are available.
```bash
ls manager.cfg worker.cfg trainer.cfg # should run without error
```

#### Prepare the configuration for the manager

The `manager.cfg` is the configuration for manager, which can be obtained by generating a standard configuration and then modifying some values as follows.
```bash
build/killallgo/killallgo_solver -gen manager.cfg # a standard config
```
Important keys for the manager:
```
# Actor
actor_num_threads=1
actor_num_parallel_games=1
actor_num_simulation=1000000
actor_mcts_value_rescale=true
actor_use_dirichlet_noise=false

# Environment
env_killallgo_ko_rule=situational # for killallgo only

# Solver
block_tt_size=24

# Manager
use_online_fine_tuning=false
use_virtual_solved=true
manager_send_and_player_job=true
manager_top_k_selection=4
manager_pcn_value_threshold=10

# Broker
use_broker=true
broker_logging=true
```

For large questions, you need to change `actor_num_simulation` to a larger value to increase the maximum tree size, e.g., ```actor_num_simulation=10000000``` (require about 80GB of RAM).

Note that some important keys will be overwritten by runtime command line arguments when launching the manager (will be introduced later), so they can be safely ignored, e.g., `manager_job_sgf`, `nn_file_name`, `broker_host`, `broker_port`.

#### Prepare the configurations for the workers and the trainer

The configurations for workers and trainer can also be generated as follows.
```bash
build/killallgo/killallgo_solver -gen worker.cfg
build/killallgo/killallgo_solver -gen trainer.cfg
```
Important keys for the workers:
```
# Actor
actor_num_threads=16
actor_num_parallel_games=48
actor_num_simulation=100000
actor_mcts_value_rescale=true
actor_use_dirichlet_noise=false

# Network
nn_file_name=/7x7KillallGo/Training/gpcn_n32_m16_empty_op/model/weight_iter_100000.pt

# Environment
env_killallgo_ko_rule=situational # for killallgo only
```
Important keys for the trainer (for Gumbel AlphaZero):
```
# Program
program_auto_seed=true

# Actor
actor_num_simulation=32
actor_mcts_value_rescale=true
actor_use_dirichlet_noise=false
actor_use_gumbel=true
actor_use_gumbel_noise=true
actor_resign_threshold=-2
actor_use_random_op=false

# Zero
zero_num_games_per_iteration=2000
zero_actor_ignored_command=reset_actors

# Learner
learner_training_step=200
learner_num_process=6

# Network
nn_file_name=
nn_num_input_channels=18
nn_input_channel_height=7
nn_input_channel_width=7
nn_num_hidden_channels=256
nn_hidden_channel_height=7
nn_hidden_channel_width=7
nn_num_action_feature_channels=1
nn_num_blocks=3
nn_num_action_channels=2
nn_action_size=50
nn_num_value_hidden_channels=256
nn_discrete_value_size=200
nn_type_name=alphazero
nn_board_evaluation_scalar=0

# Environment
env_killallgo_ko_rule=situational # for killallgo only
```

Similar to the manager, some important keys will be overwritten by runtime command line arguments when running, which will be introduced in the following sections.

#### Configure the workers using pre-configured script

Before launching the solver, the workers need to be configured accordingly.
Different configurations are required for different types of the solver. For simplicity, a script is provided for standard solver configurations with 8 GPUs (8g384w) and 4 GPUs (4g192w).

To configure the **8**g384w **B**aseline (or **O**nline fine-tuning) solver, specify `8B` (or `8O`) and two machines (machines should have properly deployed environments):
```bash
export CHAT=NV18:8888 # set the chat service location

scripts/run-workers.sh killallgo 8B NV20 NV21 # baseline using 8 GPUs

scripts/run-workers.sh killallgo 8O NV22 NV23 # online fine-tuning using 8 GPUs
```

Or, to configure the **4**g192w solvers, specify `4B` (or `4O`) with one machine:
```bash
export CHAT=NV18:8888 # set the chat service location

scripts/run-workers.sh killallgo 4B NV20 # baseline using 4 GPUs

scripts/run-workers.sh killallgo 4O NV22 # online fine-tuning using 4 GPUs
```

The script requires your confirmation during the setup. Simply press Enter to continue.
If the script gets stuck, terminate it by Ctrl+C, then check the environment and try again.

Note that the worker system can be reused for multiple opening problems.
To change the worker configuration, just run the script again.

> **Warning**: The standard baseline configuration requires approximately 120GB of RAM on each machine. Specify a smaller ```actor_num_simulation``` (default 100000) to reduce the required memory: `scripts/run-workers.sh killallgo 4B NV20 -conf_str actor_num_simulation=10000`

Once the workers are configured, they can be accessed by the broker on the first specified machine. The broker shares the same machine index but named start with prefix `b`.
For examples, broker `b20` will be available when configuring with `8B NV20 NV21`; broker `b24` will be available when configuring with `4O NV22`.

For detailed specifications, run `scripts/run-workers.sh` without arguments.

#### Configure the workers manually

You have to configure the workers manually if `run-workers.sh` does not cover your target configuration. However, you may just changing `worker.cfg` (or using `run-workers.sh' with '-conf_str`) when you are using the same GPU node topology.

This example assumes that you are setting a 4g256w configuration using broker `b1` and four workers `W1-0`, `W1-1`, `W1-2`, and `W1-3`. Note that it is necessary to [deploy the environment correctly](#Deploy-the-environment-maunally) before configuring the workers.

For 4g256w baseline solver, configure the workers using the [nc terminal](#Check-service-availability) as follows.
```
# commands should be run step by step, do NOT paste them at the same time

# kill running processes (skip if you are sure there are no running processes)
W1-0 << solver kill
W1-1 << solver kill
W1-2 << solver kill
W1-3 << solver kill

# unlink previous broker and workers
b1 << operate discard workers
W1-0 << unset broker
W1-1 << unset broker
W1-2 << unset broker
W1-3 << unset broker

# link broker and workers
W1-0 << set broker b1
W1-1 << set broker b1
W1-2 << set broker b1
W1-3 << set broker b1

# check broker status, should be idle 0/4
b1 << query status

# set commands for solver processes
W1-0 << solver cmdline build/killallgo/killallgo_solver -conf_file worker.cfg -mode worker -conf_str "actor_num_parallel_games=64"
W1-1 << solver cmdline build/killallgo/killallgo_solver -conf_file worker.cfg -mode worker -conf_str "actor_num_parallel_games=64"
W1-2 << solver cmdline build/killallgo/killallgo_solver -conf_file worker.cfg -mode worker -conf_str "actor_num_parallel_games=64"
W1-3 << solver cmdline build/killallgo/killallgo_solver -conf_file worker.cfg -mode worker -conf_str "actor_num_parallel_games=64"

# launch solver processes
W1-0 << solver launch
W1-1 << solver launch
W1-2 << solver launch
W1-3 << solver launch

# wait a while for launching solver processes
# check broker status again, should become idle 0/256
b1 << query status

# perform worker warm-up (copy the "request solve" command 256 times and paste them at the same time)
b1 << request solve "(;FF[4]CA[UTF-8]AP[GoGui:1.5.1]SZ[7]KM[6.5]DT[2022-05-20];B[dc];W[];B[de];W[df];B[ce];W[ee];B[ed];W[fe];B[fd])"

# wait a while until broker status become idle 0/256 again
b1 << query status

# set worker configuration flag, and the configuration completed
b1 << set solver_env 4g256w
```

On the other hand, for 4g256w online fine-tuning solver (namely 3g256w+1sp1op), since GPU 3 is reserved for the trainer, the configuration should be changed accordingly as follows.
```
# kill running processes (skip if you are sure there are no running processes)
W1-0 << solver kill
W1-1 << solver kill
W1-2 << solver kill

# unlink previous broker and workers
b1 << operate discard workers
W1-0 << unset broker
W1-1 << unset broker
W1-2 << unset broker

# link broker and workers, note that W1-3 is NOT linked
W1-0 << set broker b1
W1-1 << set broker b1
W1-2 << set broker b1

# check broker status, should be idle 0/3
b1 << query status

# set commands for solver processes (86w + 85w + 85w = 256w)
W1-0 << solver cmdline build/killallgo/killallgo_solver -conf_file worker.cfg -mode worker -conf_str "actor_num_parallel_games=86"
W1-1 << solver cmdline build/killallgo/killallgo_solver -conf_file worker.cfg -mode worker -conf_str "actor_num_parallel_games=85"
W1-2 << solver cmdline build/killallgo/killallgo_solver -conf_file worker.cfg -mode worker -conf_str "actor_num_parallel_games=85"

# launch solver processes
W1-0 << solver launch
W1-1 << solver launch
W1-2 << solver launch

# wait a while for launching solver processes
# check broker status again, should become idle 0/256
b1 << query status

# perform worker warm-up (copy the "request solve" command 256 times and paste them at the same time)
b1 << request solve "(;FF[4]CA[UTF-8]AP[GoGui:1.5.1]SZ[7]KM[6.5]DT[2022-05-20];B[dc];W[];B[de];W[df];B[ce];W[ee];B[ed];W[fe];B[fd])"

# wait a while until broker status become idle 0/256 again
b1 << query status

# set worker configuration flag, and the configuration completed
b1 << set solver_env 3g256w+1sp1op
```

Once the workers are configured, you may [launch the manager](#Launch-the-manager-and-the-trainer) using broker `b1`. However, if workers failed to be launched, check log files inside folder `solver-wrapper.archive`.

#### Launch the manager (and the trainer)

To launch the solver, run the script as follows.
```bash
# run scripts/start-container.sh to launch the container first

# to run a baseline solver:
scripts/run-manager.sh killallgo JA CHAT=NV18:8888 BROKER=b20 CVD=0 ONLINE=false

# or, to run an online solver:
scripts/run-manager.sh killallgo JA CHAT=NV18:8888 BROKER=b22 CVD=1 ONLINE=true
```

* `killallgo` specifies the game type, followed by one or more opening to be solved, e.g., `JA`. The corresponding SGF must be defined in `.opening.txt`.
* `CHAT` specifies the chat service.
* `BROKER` specifies the broker name. The broker (and its associated workers) must be pre-configured as introduced in the previous sections.
* `CVD` is an alias of `CUDA_VISIBLE_DEVICES`, specifying the GPUs on which to run the manager. Note that the manager requires only one GPU.
* `ONLINE` specifies whether to use online fine-tuning. The broker must be configured to match the scheme, e.g., `ONLINE=false` requires a baseline configuration like `8B`.

#### Specify other configurations

The solver loads other unspecified configurations from the built-in defaults and the configuration file (`manager.cfg`). You may specify configuration strings directly as follows.
```bash
scripts/run-manager.sh killallgo JA BROKER=b20 CVD=0 ONLINE=false actor_num_simulation=10000000 manager_pcn_value_threshold=16.5
```

* `SIM` specifies `actor_num_simulation` for the manager, default 5000000.
* `PCN` specifies `manager_pcn_value_threshold` for the manager, default 16.5.
* `MODEL` specifies the network model, default `gpcn_n32_m16_empty_op:100000`. E.g., `MODEL=7x7_pcn_v1:100000`. Note that setting `MODEL=$NAME:$ITER` is equivalent to setting `nn_file_name=$NN_HOME/$NAME/model/weight_iter_$ITER.pt`. Run `scripts/run-manager.sh` without arguments to check current `NN_HOME` location.
* `TIMEOUT` sets the maximum solving time in seconds, default unlimited. As the timer is inaccurate, at least 60s must be added to ensure the running time, e.g., use `TIMEOUT=10060` to ensure the solver runs for at least 10000 seconds.

Note that built-in defaults (hardcoded in `run-manager.sh`) always overwrite configurations in `manager.cfg`. To overwrite them, you must use command line arguments (or modify the script).

For detailed specifications, run `scripts/run-manager.sh` without arguments.

#### Handle solver results

For each problem, `run-manager.sh` displays its name and solving time, e.g.,
```
JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op ... 32105s
```
The solving time, e.g., `32105s`, is not displayed until the problem is completed.
If you specify multiple questions, they will be run sequentially.

To terminate a running solver (e.g., launched with incorrect arguments), simply use Ctrl+C to kill the `run-manager.sh` script. Once the script ends, all unfinished worker jobs are killed. For online fine-tuning, the trainer is also killed.

The solver stores the results in files named with `$PREFIX`, where `$PREFIX` is automatically generated as `[OPENING_ID]-[SOLVER_MODE]-[NUM_SIMULATION]-[PCN_THRESHOLD]-[ENV_LABEL]`.

Note that to avoid accidentally overwriting previous experiment results, the manager will not start when result files with the same name already exists.
You can use the following flags with `run-manager.sh` to change the default behavior.
* `OVERWRITE` allows the solver to overwrite existing results, default false.
* `TAG` adds a string before `$PREFIX` for result files, i.e., `PREFIX=$TAG-$PREFIX`.
* `INFO` adds a string after `$PREFIX` for result files, i.e., `PREFIX=$PREFIX-$INFO`.

After a successful run, five files are stored: `$PREFIX.log`, `$PREFIX.stat`, `$PREFIX.jobs`, `$PREFIX.sgf`, and `$PREFIX.7esgf`. For online fine-tuning, the network models are also store in `$NN_HOME/online_$PREFIX`. For example, the results of solving `JA` with online fine-tuning solver:
```
# scripts/run-manager.sh killallgo JA BROKER=b22 CVD=0 ONLINE=true SIM=10000000

# five files are directly stored in the working folder
JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op.log
JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op.sgf
JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op.7esgf
JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op.stat
JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op.jobs

# network models are stored in a folder in $NN_HOME
online_JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op
```

The information stored in these five files:
* `.log` stores the log output by the manager.
* `.sgf` (or `.7esgf`) stores the search tree. Use GoGui (or CGI lab Editor) to open the file.
* `.stat` stores the main result and statistics. The file contains only one line with 37 records separated by space. An example is attached below.
```
loss 32105 1413138 281648 12227752.645 43.4150 1728286526 6136.3351 279363 0 1908 377 10179 1377 0 4745 276903 208 99.18% 0% .67% .13% 1.68% 380 384 66 82 68 110 720 592 626 1147 498 4037 4745 550997
```
| #     | Example             | Description                       |
| :---- |:------------------- |:--------------------------------- |
| 1     | loss                | solver result                     |
| 2     | 32105               | time usage                        |
| 3     | 1413138             | manager node count (tree size)    |
| 4     | 281648              | number of jobs                    |
| 5     | 12227752.645        | time usage for solving jobs       |
| 6     | 43.4150             | average time per job              |
| 7     | 1728286526          | nodes for solving jobs            |
| 8     | 6136.3351           | average node count per job        |
| 9-12  | 279363 0 1908 377   | numbers of job results: win, loss, unsolved, unfinished |
| 13-17 | 10179 ... 276903    | numbers of manager event flags: <br>"already solved", "unsolved!", "black wins!", <br>"is_root_virtual_solved_1", "is_root_virtual_solved_0" |
| 18    | 208                 | number of "load_model" commands   |
| 19-22 | 99.18% 0% .67% .13% | percentages of job results: win, loss, unsolved, unfinished |
| 23    | 1.68%               | percentages of root virtual solved |
| 24-25 | 380 384             | average and maximum loading (number of running jobs) |
| 26-37 | 66 82 ... 550997    | numbers of observed loading: 0-32, 33-64, ..., 353-384 jobs |

* `.jobs` stores all jobs generated by the manager. Each job is stored using one line as follows.
```
1 (;FF[4]CA[UTF-8]SZ[7];B[dc];W[];B[de];W[df];B[ce];W[ee];B[ed];W[fe];B[ff];W[fg];B[gf];W[ef]) 16.1295 1 40c1cffff 1114 "" 14585ms
```
| #   | Example             | Description                          |
|:--- |:------------------- |:------------------------------------ |
| 1   | 1                   | job ID                               |
| 2   | (;FF[4] ... ;W[ef]) | job SGF                              |
| 3   | 16.1295             | job PCN value (estimated by manager) |
| 4   | 1                   | job result: status                   |
| 5   | 40c1cffff           | job result: R-Zone info              |
| 6   | 1114                | job result: node count (tree size)   |
| 7   | ""                  | job result: GHI info                 |
| 8   | 14585ms             | job result: time usage               |
