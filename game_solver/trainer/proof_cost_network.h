#pragma once

#include "configuration.h"
#include "network.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gamesolver {

class ProofCostNetworkOutput : public minizero::network::NetworkOutput {
public:
    float value_n_;
    float value_m_;
    std::vector<float> policy_;
    std::vector<float> policy_logits_;
    std::vector<float> board_evaluation_;

    ProofCostNetworkOutput(int policy_size)
    {
        value_n_ = value_m_ = 0.0f;
        policy_.resize(policy_size, 0.0f);
        policy_logits_.resize(policy_size, 0.0f);
        board_evaluation_.resize(policy_size - 1, 0.0f);
    }
};

class ProofCostNetwork : public minizero::network::Network {
public:
    ProofCostNetwork()
    {
        batch_size_ = 0;
        clearTensorInput();
    }

    void loadModel(const std::string& nn_file_name, const int gpu_id) override
    {
        Network::loadModel(nn_file_name, gpu_id);
        std::vector<torch::jit::IValue> dummy;
        value_size_ = network_.get_method("get_value_size")(dummy).toInt();
        batch_size_ = 0;
        minizero::config::nn_action_size = action_size_;
        minizero::config::nn_discrete_value_size = value_size_;
    }

    std::string toString() const override
    {
        std::ostringstream oss;
        oss << Network::toString();
        return oss.str();
    }

    int pushBack(std::vector<float> features)
    {
        assert(static_cast<int>(features.size()) == getNumInputChannels() * getInputChannelHeight() * getInputChannelWidth());

        int index;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            index = batch_size_++;
            tensor_input_.resize(batch_size_);
        }
        tensor_input_[index] = torch::from_blob(features.data(), {1, getNumInputChannels(), getInputChannelHeight(), getInputChannelWidth()}).clone();
        return index;
    }

    std::vector<std::shared_ptr<minizero::network::NetworkOutput>> forward()
    {
        assert(batch_size_ > 0);
        auto forward_result = network_.forward(std::vector<torch::jit::IValue>{torch::cat(tensor_input_).to(getDevice())}).toGenericDict();

        auto policy_output = forward_result.at("policy").toTensor().to(at::kCPU);
        auto policy_logits_output = forward_result.at("policy_logit").toTensor().to(at::kCPU);
        auto value_n_output = forward_result.at("value_n").toTensor().to(at::kCPU);
        auto value_m_output = forward_result.at("value_m").toTensor().to(at::kCPU);
        auto board_evaluate_output = (forward_result.contains("board_evaluation")) ? forward_result.at("board_evaluation").toTensor().to(at::kCPU) : torch::zeros(batch_size_ * (getActionSize() - 1));
        assert(policy_output.numel() == batch_size_ * getActionSize());
        assert(policy_logits_output.numel() == batch_size_ * getActionSize());
        assert(value_n_output.numel() == batch_size_ * getValueSize());
        assert(value_m_output.numel() == batch_size_ * getValueSize());
        assert(board_evaluate_output.numel() == batch_size_ * (getActionSize() - 1));

        const int policy_size = getActionSize();
        const int board_evaluation_size = getActionSize() - 1;
        std::vector<std::shared_ptr<minizero::network::NetworkOutput>> network_outputs;
        for (int i = 0; i < batch_size_; ++i) {
            network_outputs.emplace_back(std::make_shared<ProofCostNetworkOutput>(policy_size));
            auto proof_cost_network_output = std::static_pointer_cast<ProofCostNetworkOutput>(network_outputs.back());
            std::copy(policy_output.data_ptr<float>() + i * policy_size,
                      policy_output.data_ptr<float>() + (i + 1) * policy_size,
                      proof_cost_network_output->policy_.begin());
            std::copy(policy_logits_output.data_ptr<float>() + i * policy_size,
                      policy_logits_output.data_ptr<float>() + (i + 1) * policy_size,
                      proof_cost_network_output->policy_logits_.begin());
            std::copy(board_evaluate_output.data_ptr<float>() + i * board_evaluation_size,
                      board_evaluate_output.data_ptr<float>() + (i + 1) * board_evaluation_size,
                      proof_cost_network_output->board_evaluation_.begin());

            // change value distribution to scalar
            int value_start = 0;
            proof_cost_network_output->value_n_ = std::accumulate(value_n_output.data_ptr<float>() + i * getValueSize(),
                                                                  value_n_output.data_ptr<float>() + (i + 1) * getValueSize(),
                                                                  0.0f,
                                                                  [&value_start](const float& sum, const float& value) { return sum + value * value_start++; });
            value_start = 0;
            proof_cost_network_output->value_m_ = std::accumulate(value_m_output.data_ptr<float>() + i * getValueSize(),
                                                                  value_m_output.data_ptr<float>() + (i + 1) * getValueSize(),
                                                                  0.0f,
                                                                  [&value_start](const float& sum, const float& value) { return sum + value * value_start++; });
        }

        batch_size_ = 0;
        clearTensorInput();
        return network_outputs;
    }

    inline int getValueSize() const { return value_size_; }
    inline int getBatchSize() const { return batch_size_; }

private:
    inline void clearTensorInput()
    {
        tensor_input_.clear();
        tensor_input_.reserve(kReserved_batch_size);
    }

    int value_size_;
    int batch_size_;
    std::mutex mutex_;
    std::vector<torch::Tensor> tensor_input_;

    const int kReserved_batch_size = 4096;
};

} // namespace gamesolver
