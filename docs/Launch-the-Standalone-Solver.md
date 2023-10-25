## Launch the Standalone Solver

The standalone solver uses a single solver process to solve relatively simple problems. 
Although each problem is solved by a single thread, the solver process typically uses multiple threads to efficiently solve problems in parallel.
* [Build and Launch](#Build-and-Launch)
* [Input/Output](#InputOutput)

### Build and Launch

* Build and compile the project
`./scripts/build.sh killallgo`

* Generate a default config
`build/killallgo/killallgo_solver -gen test.cfg`

* The following parameters are required for solving 7x7-killall-Go when setting the config file:

```
# actor
actor_num_threads=4 # Number of threads for the worker to iterate the games
actor_num_parallel_games=32 # Number of games when running in parallel
actor_num_simulation=10000 # The maximum simulations to solve a problem
actor_mcts_value_rescale=true # True when using the PCN network
actor_use_dirichlet_noise=false # Better to set false for deterministic behaviors

# network
nn_file_name=/7x7KillallGo/Training/7x7_pcn_v2/model/weight_iter_100000.pt # The model file location


env_killallgo_ko_rule=situational # must be situational
env_board_size=7 # must be 7
solved_player=W # must be W
use_rzone=true # true for enabling rzone
use_block_tt=true # true for enabling block based zone pattern table
block_tt_size=16 # 16 means maximum 2^16 entries for the zone pattern table
log_solver_sgf=false # true for logging the solution tree when the search is done
solver_output_directory=result # where the solution tree are stored
use_ghi_check=true # true for checking GHI problems in rzone
```

* Run the worker
`build/killallgo/killallgo_solver -mode worker -conf_file test.cfg`

* For a convenient usage, if you want to specify some configurations without modifying the .cfg files, for exmaples, to set actor_num_parallel_games=16 and actor_num_simulation=20000, use the following format of command.
```
build/killallgo/killallgo_solver -mode worker -conf_file test.cfg -conf_str actor_num_parallel_games=16:actor_num_simulation=20000
```

### Input/Output

* A problem is in the follwoing format, just send them to the standard input.
```
+ JobID sgf_string [optional arguments]
```
e.g.
```
+ 1 (;FF[4]CA[UTF-8]SZ[7];B[dc];W[];B[de];W[df];B[ce];W[ee];B[ed];W[fe];B[ff];W[fg];B[gf];W[ef])
+ 2 (;FF[4]CA[UTF-8]SZ[7];B[dc];W[];B[de];W[df];B[ce];W[ee];B[fd];W[ed];B[ec];W[fe];B[gc];W[ff];B[dd])
```
* [arguments arguments] are anything you want to input as arguments

* When a search is done a problem, an output for a correspoding job_id is shown on the screen in the following format:
job_id status rzone nodes "ghi_string"
e.g.
```
1 1 2060c1cffff 1513 ""
2 -1 a1c3e7c 245 ""
```
* After the search is done, check the results on the console or traverse the solution tree in solver_output_directory
