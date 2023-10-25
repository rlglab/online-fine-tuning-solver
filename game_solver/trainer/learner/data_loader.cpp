#include "data_loader.h"
#include "configuration.h"
#include "environment.h"
#include "gs_configuration.h"
#include "rotation.h"
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace gamesolver {

using namespace minizero;
using namespace minizero::utils;

DataLoader::DataLoader(std::string conf_file_name)
{
    config::ConfigureLoader cl;
    gamesolver::setConfiguration(cl);
    cl.loadFromFile(conf_file_name);
    env::setUpEnv();
}

void DataLoader::loadDataFromFile(const std::string& file_name)
{
    std::ifstream fin(file_name, std::ifstream::in);
    for (std::string content; std::getline(fin, content);) {
        EnvironmentLoader env_loader;
        if (!env_loader.loadFromString(content)) { continue; }
        int total_length = (env_loaders_.empty() ? 0 : env_loaders_.back().second);
        int opening_length = (env_loader.getTag("OP") == "" ? 0 : std::stoi(env_loader.getTag("OP")));
        env_loaders_.push_back({env_loader, total_length + env_loader.getActionPairs().size() - opening_length});
    }
    if (gamesolver::use_solved_positions) {
        solved_sgf_env_loaders_.clear();
        std::fstream fin;
        fin.open(file_name.substr(0, file_name.find("/sgf")) + "/solved_sgf.txt", std::ios::in);
        for (std::string content; std::getline(fin, content);) {
            EnvironmentLoader env_loader;
            if (!env_loader.loadFromString(content)) { continue; }
            solved_sgf_env_loaders_.push_back(env_loader);
        }
    }
}

PCNData DataLoader::getPCNTrainingData()
{
    bool use_solved_positions = (solved_sgf_env_loaders_.size() && Random::randReal() < gamesolver::manager_solved_positions_ratio);

    int env_id = -1, pos = -1;
    if (use_solved_positions) {
        // random pickup one position from solved positions
        env_id = Random::randInt() % solved_sgf_env_loaders_.size();
        pos = solved_sgf_env_loaders_[env_id].getActionPairs().size() - 1;
    } else {
        // random pickup one position
        std::pair<int, int> p = getEnvIDAndPosition(Random::randInt() % getDataSize());
        env_id = p.first;
        pos = p.second;
        const EnvironmentLoader& env_loader = env_loaders_[env_id].first;
        int opening_length = (env_loader.getTag("OP") == "" ? 0 : std::stoi(env_loader.getTag("OP")));
        pos += opening_length;
        assert(pos < static_cast<int>(env_loader.getActionPairs().size()));
    }

    // replay the game until to the selected position
    const EnvironmentLoader& env_loader = use_solved_positions ? solved_sgf_env_loaders_[env_id] : env_loaders_[env_id].first;
    env_.reset();
    for (int i = 0; i < pos; ++i) { env_.act(env_loader.getActionPairs()[i].first); }

    // calculate training data
    PCNData data;
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
    data.features_ = env_.getFeatures(rotation);
    data.policy_ = getPolicyDistribution(env_loader, pos, rotation);

    // calculate pcn value
    float value_n = (env_loader.getReturn() == -1 ? 0 : config::nn_discrete_value_size - 1);
    float value_m = (env_loader.getReturn() == 1 ? 0 : config::nn_discrete_value_size - 1);
    for (int i = env_loader.getActionPairs().size() - 1; i >= pos; --i) {
        const Action& action = env_loader.getActionPairs()[i].first;
        if (action.getPlayer() == env::Player::kPlayer1) {
            value_n += log10(env_loader.getPolicySize());
        } else if (action.getPlayer() == env::Player::kPlayer2) {
            value_m += log10(env_loader.getPolicySize());
        }
    }

    setPCNValue(data.value_n_, fmax(0, fmin(config::nn_discrete_value_size - 1, value_n)));
    setPCNValue(data.value_m_, fmax(0, fmin(config::nn_discrete_value_size - 1, value_m)));

    // TODO: fix this
    // set board evaluation
    int board_size = gamesolver::env_board_size;
    for (size_t i = pos; i < env_loader.getActionPairs().size(); ++i) { env_.act(env_loader.getActionPairs()[i].first); }
    data.board_evaluation_.resize(board_size * board_size, 0.0f);
    // for (int pos = 0; pos < board_size * board_size; ++pos) {
    //     int rotation_pos = env_loader.getRotatePosition(pos, rotation);
    //     if (env_.getBensonBitboard().get(env::Player::kPlayer1).test(pos)) {
    //         data.board_evaluation_[rotation_pos] = 1.0f;
    //     } else if (env_.getBensonBitboard().get(env::Player::kPlayer2).test(pos)) {
    //         data.board_evaluation_[rotation_pos] = 0.0f;
    //     } else {
    //         data.board_evaluation_[rotation_pos] = 0.5f;
    //     }
    // }

    return data;
}

std::pair<int, int> DataLoader::getEnvIDAndPosition(int index) const
{
    int left = 0, right = env_loaders_.size();
    index %= env_loaders_.back().second;

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (index >= env_loaders_[mid].second) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return {left, (left == 0 ? index : index - env_loaders_[left - 1].second)};
}

std::vector<float> DataLoader::getPolicyDistribution(const EnvironmentLoader& env_loader, int pos, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/)
{
    std::vector<float> policy(env_loader.getPolicySize(), 0.0f);
    const std::string& distribution = env_loader.getActionPairs()[pos].second;
    if (distribution.empty()) {
        const Action& action = env_loader.getActionPairs()[pos].first;
        policy[env_loader.getRotatePosition(action.getActionID(), rotation)] = 1.0f;
    } else {
        float sum = 0.0f;
        std::string tmp;
        std::istringstream iss(distribution);
        while (std::getline(iss, tmp, ',')) {
            int position = env_loader.getRotatePosition(std::stoi(tmp.substr(0, tmp.find(":"))), rotation);
            float count = std::stof(tmp.substr(tmp.find(":") + 1));
            policy[position] = count;
            sum += count;
        }
        for (auto& p : policy) { p /= sum; }
    }
    return policy;
}

void DataLoader::setPCNValue(std::vector<float>& values, float value)
{
    values.clear();
    values.resize(config::nn_discrete_value_size, 0.0f);

    int value_floor = floor(value);
    int value_ceil = ceil(value);
    if (value_floor == value_ceil) {
        values[value_floor] = 1.0f;
    } else {
        values[value_floor] = value_ceil - value;
        values[value_ceil] = value - value_floor;
    }
}

} // namespace gamesolver
