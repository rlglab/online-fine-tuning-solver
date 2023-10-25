#include "configuration.h"
#include "data_loader.h"
#include "gs_configuration.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;
using namespace minizero;

class Conf {
public:
    Conf(std::string file_name)
    {
        config::ConfigureLoader cl;
        gamesolver::setConfiguration(cl);
        cl.loadFromFile(file_name);
        env::setUpEnv();
    }

    inline bool useGumbel() const { return config::actor_use_gumbel; }
    inline int getTrainingStep() const { return config::learner_training_step; }
    inline int getTrainingDisplayStep() const { return config::learner_training_display_step; }
    inline int getBatchSize() const { return config::learner_batch_size; }
    inline float getLearningRate() const { return config::learner_learning_rate; }
    inline float getMomentum() const { return config::learner_momentum; }
    inline float getWeightDecay() const { return config::learner_weight_decay; }
    inline int getNumProcess() const { return config::learner_num_process; }
    inline std::string getGameName() const { return Environment().name(); }
    inline int getNNNumInputChannels() const { return config::nn_num_input_channels; }
    inline int getNNInputChannelHeight() const { return config::nn_input_channel_height; }
    inline int getNNInputChannelWidth() const { return config::nn_input_channel_width; }
    inline int getNNNumHiddenChannels() const { return config::nn_num_hidden_channels; }
    inline int getNNHiddenChannelHeight() const { return config::nn_hidden_channel_height; }
    inline int getNNHiddenChannelWidth() const { return config::nn_hidden_channel_width; }
    inline int getNNNumActionFeatureChannels() const { return config::nn_num_action_feature_channels; }
    inline int getNNNumBlocks() const { return config::nn_num_blocks; }
    inline int getNNNumActionChannels() const { return config::nn_num_action_channels; }
    inline int getNNActionSize() const { return config::nn_action_size; }
    inline int getNNNumValueHiddenChannels() const { return config::nn_num_value_hidden_channels; }
    inline int getNNDiscreteValueSize() const { return config::nn_discrete_value_size; }
    inline float getNNBoardEvaluationScalar() const { return gamesolver::nn_board_evaluation_scalar; }
    inline bool useBroker() const { return gamesolver::use_broker; }
    inline std::string getBrokerHost() const { return gamesolver::broker_host; }
    inline int getBrokerPort() const { return gamesolver::broker_port; }
    inline std::string getBrokerName() const { return gamesolver::broker_name; }
};

PYBIND11_MODULE(pybind_pcn_learner, m)
{
    py::class_<Conf>(m, "Conf")
        .def(py::init<std::string>())
        .def("use_gumbel", &Conf::useGumbel)
        .def("get_training_step", &Conf::getTrainingStep)
        .def("get_training_display_step", &Conf::getTrainingDisplayStep)
        .def("get_batch_size", &Conf::getBatchSize)
        .def("get_learning_rate", &Conf::getLearningRate)
        .def("get_momentum", &Conf::getMomentum)
        .def("get_weight_decay", &Conf::getWeightDecay)
        .def("get_num_process", &Conf::getNumProcess)
        .def("get_game_name", &Conf::getGameName)
        .def("get_nn_num_input_channels", &Conf::getNNNumInputChannels)
        .def("get_nn_input_channel_height", &Conf::getNNInputChannelHeight)
        .def("get_nn_input_channel_width", &Conf::getNNInputChannelWidth)
        .def("get_nn_num_hidden_channels", &Conf::getNNNumHiddenChannels)
        .def("get_nn_hidden_channel_height", &Conf::getNNHiddenChannelHeight)
        .def("get_nn_hidden_channel_width", &Conf::getNNHiddenChannelWidth)
        .def("get_nn_num_action_feature_channels", &Conf::getNNNumActionFeatureChannels)
        .def("get_nn_num_blocks", &Conf::getNNNumBlocks)
        .def("get_nn_num_action_channels", &Conf::getNNNumActionChannels)
        .def("get_nn_action_size", &Conf::getNNActionSize)
        .def("get_nn_num_value_hidden_channels", &Conf::getNNNumValueHiddenChannels)
        .def("get_nn_discrete_value_size", &Conf::getNNDiscreteValueSize)
        .def("get_nn_board_evaluation_scalar", &Conf::getNNBoardEvaluationScalar)
        .def("use_broker", &Conf::useBroker)
        .def("get_broker_host", &Conf::getBrokerHost)
        .def("get_broker_port", &Conf::getBrokerPort)
        .def("get_broker_name", &Conf::getBrokerName);

    py::class_<gamesolver::DataLoader>(m, "DataLoader")
        .def(py::init<std::string>())
        .def("seed", &gamesolver::DataLoader::seed)
        .def("load_data_from_file", &gamesolver::DataLoader::loadDataFromFile)
        .def("get_pcn_training_data", [](gamesolver::DataLoader& data_loader) {
            gamesolver::PCNData data = data_loader.getPCNTrainingData();
            py::dict res;
            res["features"] = py::cast(data.features_);
            res["policy"] = py::cast(data.policy_);
            res["value_n"] = py::cast(data.value_n_);
            res["value_m"] = py::cast(data.value_m_);
            res["board_evaluation"] = py::cast(data.board_evaluation_);
            return res;
        });
}
