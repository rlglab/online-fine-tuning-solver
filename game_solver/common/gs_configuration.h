#pragma once

#include "configure_loader.h"
#include <string>

namespace gamesolver {

// trainer parameters
extern std::string trainer_opening_sgf;

// solver parameters
extern char solved_player;
extern bool use_rzone;
extern bool use_block_tt;
extern bool use_grid_tt;
extern int block_tt_size;
extern int grid_tt_size;
extern bool use_ghi_check;
extern bool use_timer_in_tt;
extern bool log_solver_sgf;
extern std::string solver_output_directory;

// manager parameters
extern bool use_online_fine_tuning;
extern bool use_virtual_solved;
extern bool manager_send_and_player_job;
extern int manager_top_k_selection;
extern float manager_pcn_value_threshold;
extern bool use_critical_positions;
extern int manager_critical_positions_n;
extern int manager_critical_positions_m;
extern bool use_solved_positions;
extern float manager_solved_positions_ratio;
extern std::string manager_job_sgf;
extern std::string tree_file_name;

// broker parameters
extern bool use_broker;
extern std::string broker_host;
extern int broker_port;
extern std::string broker_name;
extern std::string broker_adapter_name;
extern bool broker_logging;

// network parameters
extern float nn_board_evaluation_scalar;

// actor parameters
extern bool actor_use_random_op;
extern int actor_random_op_max_length;
extern bool actor_random_op_use_softmax;
extern float actor_random_op_softmax_temperature;
extern float actor_random_op_softmax_sum_limit;

// environment parameters
extern int env_board_size;

void setConfiguration(minizero::config::ConfigureLoader& cl);

} // namespace gamesolver
