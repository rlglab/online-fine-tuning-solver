#include "gs_configuration.h"
#include "configuration.h"

namespace gamesolver {

using namespace minizero;

// trainer parameters
std::string trainer_opening_sgf = "(;FF[4]CA[UTF-8]SZ[7]KM[0])";

// solver parameters
char solved_player = 'W';
bool use_rzone = true;
bool use_block_tt = true;
bool use_grid_tt = false;
int block_tt_size = 16;
int grid_tt_size = 1;
bool use_ghi_check = true;
bool use_timer_in_tt = false;
bool log_solver_sgf = false;
std::string solver_output_directory = "result";

// manager parameters
bool use_online_fine_tuning = false;
bool use_virtual_solved = true;
bool manager_send_and_player_job = true;
int manager_top_k_selection = 4;
float manager_pcn_value_threshold = 10.0f;
bool use_critical_positions = true;
int manager_critical_positions_n = 1000;
int manager_critical_positions_m = 0;
bool use_solved_positions = false;
float manager_solved_positions_ratio = 0.1;
std::string manager_job_sgf = "(;FF[4]CA[UTF-8]SZ[7];B[dc];W[];B[de];W[df];B[ce])";
std::string tree_file_name = "tree_manager";

// broker parameters
bool use_broker = false;
std::string broker_host = "NV03";
int broker_port = 8888;
std::string broker_name = "broker";
std::string broker_adapter_name = "manager";
bool broker_logging = false;

// network parameters
float nn_board_evaluation_scalar = 20.0;

// actor parameters
bool actor_use_random_op = true;
int actor_random_op_max_length = 25;
bool actor_random_op_use_softmax = true;
float actor_random_op_softmax_temperature = 2.0;
float actor_random_op_softmax_sum_limit = 0.7;

// environment parameters
int env_board_size = 7; // TODO: remove this

void setConfiguration(config::ConfigureLoader& cl)
{
    config::setConfiguration(cl);

    // trainer parameters
    cl.addParameter("trainer_opening_sgf", trainer_opening_sgf, "", "Trainer");

    // solver parameters
    cl.addParameter("solved_player", solved_player, "W for 7x7-killlal Go, B for Hex", "Solver");
    cl.addParameter("use_rzone", use_rzone, "true for enabling rzone", "Solver");
    cl.addParameter("use_block_tt", use_block_tt, "true for enabling block based zone pattern table", "Solver");
    cl.addParameter("use_grid_tt", use_grid_tt, "true for enabling grid based zone pattern table", "Solver");
    cl.addParameter("block_tt_size", block_tt_size, "16 means maximum 2^16 entries for the zone pattern table", "Solver");
    cl.addParameter("grid_tt_size", grid_tt_size, "", "Solver");
    cl.addParameter("use_timer_in_tt", use_timer_in_tt, "", "Solver");
    cl.addParameter("log_solver_sgf", log_solver_sgf, "true for logging the solution tree when the search is done", "Solver");
    cl.addParameter("solver_output_directory", solver_output_directory, "where the solution tree are stored", "Solver");
    cl.addParameter("use_ghi_check", use_ghi_check, "true for checking GHI problems in rzone", "Solver");

    // manager pararmeters
    cl.addParameter("use_online_fine_tuning", use_online_fine_tuning, "", "Manager");
    cl.addParameter("use_virtual_solved", use_virtual_solved, "", "Manager");
    cl.addParameter("manager_send_and_player_job", manager_send_and_player_job, "", "Manager");
    cl.addParameter("manager_top_k_selection", manager_top_k_selection, "select child from top k children randomly in MCTS selection", "Manager");
    cl.addParameter("manager_pcn_value_threshold", manager_pcn_value_threshold, "", "Manager");
    cl.addParameter("use_critical_positions", use_critical_positions, "", "Manager");
    cl.addParameter("manager_critical_positions_n", manager_critical_positions_n, "", "Manager");
    cl.addParameter("manager_critical_positions_m", manager_critical_positions_m, "", "Manager");
    cl.addParameter("use_solved_positions", use_solved_positions, "", "Manager");
    cl.addParameter("manager_solved_positions_ratio", manager_solved_positions_ratio, "", "Manager");
    cl.addParameter("manager_job_sgf", manager_job_sgf, "", "Manager");
    cl.addParameter("tree_file_name", tree_file_name, "", "Manager");

    // broker parameters
    cl.addParameter("use_broker", use_broker, "", "Broker");
    cl.addParameter("broker_host", broker_host, "", "Broker");
    cl.addParameter("broker_port", broker_port, "", "Broker");
    cl.addParameter("broker_name", broker_name, "", "Broker");
    cl.addParameter("broker_adapter_name", broker_adapter_name, "", "Broker");
    cl.addParameter("broker_logging", broker_logging, "", "Broker");

    // network parameters
    cl.addParameter("nn_board_evaluation_scalar", nn_board_evaluation_scalar, "", "Network");

    // actor parameters
    cl.addParameter("actor_use_random_op", actor_use_random_op, "", "Actor");
    cl.addParameter("actor_random_op_max_length", actor_random_op_max_length, "", "Actor");
    cl.addParameter("actor_random_op_use_softmax", actor_random_op_use_softmax, "", "Actor");
    cl.addParameter("actor_random_op_softmax_temperature", actor_random_op_softmax_temperature, "", "Actor");
    cl.addParameter("actor_random_op_softmax_sum_limit", actor_random_op_softmax_sum_limit, "", "Actor");

    // environment parameters
    cl.addParameter("env_board_size", env_board_size, "", "Environment");
}

} // namespace gamesolver
