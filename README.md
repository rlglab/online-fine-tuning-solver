# Game Solving with Online Fine-Tuning

This is the official repository of the NeurIPS 2023 paper [Game Solving with Online Fine-Tuning](https://rlg.iis.sinica.edu.tw/papers/neurips2023-online-fine-tuning-solver).

If you use this work for research, please consider citing our paper as follows:
```
@inproceedings{
  wu2023game,
  title={Game Solving with Online Fine-Tuning},
  author={Wu, Ti-Rong and Guei, Hung and Wei, Ting Han and Shih, Chung-Chin and Chin, Jui-Te and Wu, I-Chen},
  booktitle={Thirty-seventh Conference on Neural Information Processing Systems},
  year={2023},
  url={https://openreview.net/forum?id=hN4qpvGzWn}
}
```

The following instructions are prepared for reproducing the main experiments (baseline, online-cp, online-sp, online-sp+cp) in the NeurIPS paper.

### Prerequisites

The game solver program requires a Linux operating system and at least one NVIDIA GPU to operate.

Nevertheless, to reproduce the main experiments in the NeurIPS paper, it is assume that there are three machines, `HOST1`, `HOST2`, and `HOST3`, each of which meets the requirements:
* At least 16 CPU threads and 192G RAM.
* Four NVIDIA GPU cards (GTX 1080 Ti or above).
* Properly installed NVIDIA drivers, [`podman`](https://podman.io) (or [`docker`](https://www.docker.com)), and [`tmux`](https://github.com/tmux/tmux).

Note that these are not strict requirements of the program.
You may run the solver on a single host with only one GPU installed, just use the same machine for `HOST1`, `HOST2`, and `HOST3` in the instructions below.

### Build Programs

Clone this repository with the required submodules:
```bash
git clone --recursive --branch neurips2023 git@github.com:rlglab/online-fine-tuning-solver.git
cd online-fine-tuning-solver
```

Enter the container to build the required executables:
```bash
scripts/start-container.sh

# run the below build commands inside the container
scripts/build.sh killallgo # build the main game solver program
cd chat && make && cd .. # build the message service for distributed computing

exit # exit the container
```

### Setup Distributed Computing Nodes

Now we will set up three computing nodes for the distributed game solver.
In the instructions below, remember to change `HOST1`, `HOST2`, and `HOST3` to your actual machine host names.

Launch a new `tmux` window and start the `chat` service on `HOST1`:
```bash
scripts/chat/start-service.sh 8888 # use port 8888
```
> **Hint**: Once the script has been started successfully, it outputs a single line showing the executed command. Just detach the relevant `tmux` window and let it run in the background.

Deploy the distributed computing nodes on `HOST2` and `HOST3`:
```bash
# run this script on both HOST2 and HOST3
scripts/setup-solver-tmux-session.sh CHAT=HOST1:8888
```
> **Hint**: The script launches a new `tmux` session named `solver`. Just simply detach it as previously.

Configure the required workers for either the baseline or the online fine-tuning solver on `HOST1` as follows:
```bash
export CHAT=HOST1:8888
# run this for the baseline solver:
yes | scripts/run-workers.sh killallgo "8B" HOST2 HOST3
# or, run this for the online fine-tuning solver:
yes | scripts/run-workers.sh killallgo "8O" HOST2 HOST3
```
> **Hint**: After the successful configuration, the script prints the broker name required for subsequent steps. E.g., `b2 has been configured ...` indicates a broker named `b2`.

### Solve Opening Problems

Finally, start the container to solve opening problems on `HOST1` as follows. 

```bash
scripts/start-container.sh
# run the below commands inside the container

# to solve JA using the baseline solver (remember to set CHAT and BROKER accordingly)
scripts/run-manager.sh killallgo JA CHAT=HOST1:8888 BROKER=b2 CUDA_VISIBLE_DEVICES=0 TIMEOUT=173000 actor_num_simulation=10000000 use_online_fine_tuning=false

# to solve JA using the online fine-tuning solver (online-cp)
scripts/run-manager.sh killallgo JA CHAT=HOST1:8888 BROKER=b2 CUDA_VISIBLE_DEVICES=0 TIMEOUT=173000 INFO=cp actor_num_simulation=10000000 use_online_fine_tuning=true use_critical_positions=true use_solved_positions=false
# to run online-sp or online-sp+cp solver, set these configs accordingly: INFO, use_critical_positions, and use_solved_positions
```
> **Hint**: To solve the opening using the baseline (or online fine-tuning) solver, the computing nodes must be configured accordingly by running `run-workers.sh` with `8B` (or `8O`) before running `run-manager.sh`.

Results are stored after a successful run. For example, the results of solving `JA` with the online fine-tuning solver (where `$NAME` is `JA-online-10000000-16.5-gpcn_n32_m16_empty_op-100000-7g384w+1sp1op-cp`):

* `$NAME.log` stores the log output by the manager.
* `$NAME.sgf` stores the search tree. Use GoGui to open the file.
* `$NAME.stat` stores the main result and statistics. Use `scripts/extract-main-stat.sh $NAME.stat` to extract fields as shown in the paper.
* `$NAME.jobs` stores all jobs generated by the manager.
* `training/online_$NAME` stores the network models generated during the online fine-tuning.

### Detailed Instructions

1. [Development Environment](docs/Development-Environment.md) covers the basic of developing with containers and building programs.
2. [Train PCN Models for the Solver](docs/Train-PCN-Models-for-the-Solver.md) introduces on the training of Proof Cost Networks.
3. [Launch the Standalone Solver](docs/Launch-the-Standalone-Solver.md) instructs how to solve problems using the worker.
4. [Launch the Distributed Solver](docs/Launch-the-Distributed-Solver.md) instructs how to solve problems using the manager, the workers, and the learner.
5. [Tools](docs/Tools.md) describes the usage of miscellaneous tools related to the game solvers.

### References
* [Game Solving with Online Fine-Tuning](https://openreview.net/forum?id=hN4qpvGzWn)
* [A Novel Approach to Solving Goal-Achieving Problems for Board Games](https://ojs.aaai.org/index.php/AAAI/article/view/21278/21027)
* [AlphaZero-based Proof Cost Network to Aid Game Solving](https://openreview.net/forum?id=nKWjE4QF1hB)
* [A Local-Pattern Related Look-Up Table](https://doi.org/10.1109/TG.2023.3263536)
