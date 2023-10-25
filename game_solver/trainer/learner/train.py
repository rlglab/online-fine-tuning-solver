#!/usr/bin/env python

import sys
import time
import torch
import torch.nn as nn
import torch.optim as optim
import socket
import numpy as np
from torch.utils.data import DataLoader
from torch.utils.data import IterableDataset
from torch.utils.data import get_worker_info
from proof_cost_network import create_proof_cost_network


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


class MinizeroDataset(IterableDataset):
    def __init__(self, training_dir, start_iter, end_iter, conf, conf_file_name):
        self.training_dir = training_dir
        self.start_iter = start_iter
        self.end_iter = end_iter
        self.conf = conf
        self.conf_file_name = conf_file_name
        self.data_loader = pybind_pcn_learner.DataLoader(self.conf_file_name)
        for i in range(self.start_iter, self.end_iter + 1):
            self.data_loader.load_data_from_file(f"{self.training_dir}/sgf/{i}.sgf")

    def __iter__(self):
        self.data_loader.seed(get_worker_info().id)
        while True:
            result_dict = self.data_loader.get_pcn_training_data()
            features = torch.FloatTensor(result_dict["features"]).view(self.conf.get_nn_num_input_channels(),
                                                                       self.conf.get_nn_input_channel_height(),
                                                                       self.conf.get_nn_input_channel_width())
            policy = torch.FloatTensor(result_dict["policy"])
            value_n = torch.FloatTensor(result_dict["value_n"])
            value_m = torch.FloatTensor(result_dict["value_m"])
            board_evaluation = torch.FloatTensor(result_dict["board_evaluation"])
            yield features, policy, value_n, value_m, board_evaluation


def load_model(training_dir, model_file, conf):
    # training_step, network, device, optimizer, scheduler
    training_step = 0
    network = create_proof_cost_network(conf.get_game_name(),
                                        conf.get_nn_num_input_channels(),
                                        conf.get_nn_input_channel_height(),
                                        conf.get_nn_input_channel_width(),
                                        conf.get_nn_num_hidden_channels(),
                                        conf.get_nn_hidden_channel_height(),
                                        conf.get_nn_hidden_channel_width(),
                                        conf.get_nn_num_blocks(),
                                        conf.get_nn_num_action_channels(),
                                        conf.get_nn_action_size(),
                                        conf.get_nn_num_value_hidden_channels(),
                                        conf.get_nn_discrete_value_size())
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    network.to(device)
    optimizer = optim.SGD(network.parameters(),
                          lr=conf.get_learning_rate(),
                          momentum=conf.get_momentum(),
                          weight_decay=conf.get_weight_decay())
    scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=1000000, gamma=0.1)

    if model_file:
        snapshot = torch.load(f"{training_dir}/model/{model_file}", map_location=torch.device('cpu'))
        training_step = snapshot['training_step']
        network.load_state_dict(snapshot['network'])
        optimizer.load_state_dict(snapshot['optimizer'])
        optimizer.param_groups[0]["lr"] = conf.get_learning_rate()
        scheduler.load_state_dict(snapshot['scheduler'])

    return training_step, network, device, optimizer, scheduler


def save_model(training_step, network, optimizer, scheduler, training_dir):
    snapshot = {'training_step': training_step,
                'network': network.module.state_dict(),
                'optimizer': optimizer.state_dict(),
                'scheduler': scheduler.state_dict()}
    torch.save(snapshot, f"{training_dir}/model/weight_iter_{training_step}.pkl")
    torch.jit.script(network.module).save(f"{training_dir}/model/weight_iter_{training_step}.pt")


def calculate_loss(
        output_policy_logit,
        output_value_n_logit,
        output_value_m_logit,
        output_board_evaluation,
        label_policy,
        label_value_n,
        label_value_m,
        label_board_evaluation,
        board_evaluation_scalar):
    if conf.use_gumbel():
        loss_policy = nn.functional.kl_div(nn.functional.log_softmax(output_policy_logit, dim=1), label_policy, reduction='batchmean')
    else:
        loss_policy = -(label_policy * nn.functional.log_softmax(output_policy_logit, dim=1)).sum() / output_policy_logit.shape[0]
    loss_value_n = -(label_value_n * nn.functional.log_softmax(output_value_n_logit, dim=1)).sum() / output_value_n_logit.shape[0]
    loss_value_m = -(label_value_m * nn.functional.log_softmax(output_value_m_logit, dim=1)).sum() / output_value_m_logit.shape[0]
    loss_board_evaluation = torch.nn.functional.mse_loss(output_board_evaluation, label_board_evaluation, reduction='mean') * board_evaluation_scalar
    return loss_policy, loss_value_n, loss_value_m, loss_board_evaluation


def add_training_info(training_info, key, value):
    if key not in training_info:
        training_info[key] = 0
    training_info[key] += value


def calculate_accuracy(output, label, batch_size):
    max_output = np.argmax(output.to('cpu').detach().numpy(), axis=1)
    max_label = np.argmax(label.to('cpu').detach().numpy(), axis=1)
    return (max_output == max_label).sum() / batch_size


def send_model_to_broker(training_dir, training_step, conf):
    if not conf.use_broker():
        return
    broker = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    broker.connect((conf.get_broker_host(), conf.get_broker_port()))
    message = f"{conf.get_broker_name()} << solver load_model {training_dir}/model/weight_iter_{training_step}.pt\n"
    broker.send(message.encode())
    broker.close()


def train(training_dir, conf_file_name, conf, model_file, start_iter, end_iter):
    training_step, network, device, optimizer, scheduler = load_model(training_dir, model_file, conf)
    network = nn.DataParallel(network)

    if start_iter == -1:
        save_model(training_step, network, optimizer, scheduler, training_dir)
        return

    # create dataset & dataloader
    dataset = MinizeroDataset(training_dir, start_iter, end_iter, conf, conf_file_name)
    data_loader = DataLoader(dataset, batch_size=conf.get_batch_size(), num_workers=conf.get_num_process())
    data_loader_iterator = iter(data_loader)

    training_info = {}
    for i in range(1, conf.get_training_step() + 1):
        optimizer.zero_grad()

        features, label_policy, label_value_n, label_value_m, label_board_evaluation = next(data_loader_iterator)
        network_output = network(features.to(device))
        output_policy_logit, output_value_n_logit, output_value_m_logit = network_output["policy_logit"], network_output["value_n_logit"], network_output["value_m_logit"]
        output_board_evaluation = torch.zeros(label_board_evaluation.shape, device=device) if "board_evaluation" not in network_output else network_output["board_evaluation"]
        loss_policy, loss_value_n, loss_value_m, loss_board_evaluation = calculate_loss(output_policy_logit,
                                                                                        output_value_n_logit,
                                                                                        output_value_m_logit,
                                                                                        output_board_evaluation,
                                                                                        label_policy.to(device),
                                                                                        label_value_n.to(device),
                                                                                        label_value_m.to(device),
                                                                                        label_board_evaluation.to(device),
                                                                                        conf.get_nn_board_evaluation_scalar())
        loss = loss_policy + loss_value_n + loss_value_m + loss_board_evaluation

        # record training info
        add_training_info(training_info, 'loss_policy', loss_policy.item())
        add_training_info(training_info, 'accuracy_policy', calculate_accuracy(output_policy_logit, label_policy, conf.get_batch_size()))
        add_training_info(training_info, 'loss_value_n', loss_value_n.item())
        add_training_info(training_info, 'accuracy_value_n', calculate_accuracy(output_value_n_logit, label_value_n, conf.get_batch_size()))
        add_training_info(training_info, 'loss_value_m', loss_value_m.item())
        add_training_info(training_info, 'accuracy_value_m', calculate_accuracy(output_value_m_logit, label_value_m, conf.get_batch_size()))
        add_training_info(training_info, 'loss_board_evaluation', loss_board_evaluation.item())

        loss.backward()
        optimizer.step()
        scheduler.step()

        training_step += 1
        if training_step != 0 and training_step % conf.get_training_display_step() == 0:
            eprint("[{}] nn step {}, lr: {}.".format(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), training_step, round(optimizer.param_groups[0]["lr"], 6)))
            for loss in training_info:
                eprint("\t{}: {}".format(loss, round(training_info[loss] / conf.get_training_display_step(), 5)))
            training_info = {}

    save_model(training_step, network, optimizer, scheduler, training_dir)
    send_model_to_broker(training_dir, training_step, conf)
    print("Optimization_Done", training_step)
    eprint("Optimization_Done", training_step)


if __name__ == '__main__':
    if len(sys.argv) == 4:
        game_type = sys.argv[1]
        training_dir = sys.argv[2]
        conf_file_name = sys.argv[3]

        # import pybind library
        _temps = __import__(f'build.{game_type}', globals(), locals(), ['pybind_pcn_learner'], 0)
        pybind_pcn_learner = _temps.pybind_pcn_learner
    else:
        eprint("python train.py game_type training_dir conf_file")
        exit(0)

    conf = pybind_pcn_learner.Conf(conf_file_name)

    while True:
        try:
            command = input()
            if command == "keep_alive":
                continue
            elif command == "quit":
                eprint(f"[command] {command}")
                exit(0)
            eprint(f"[command] {command}")
            model_file, start_iter, end_iter = command.split()
            train(training_dir, conf_file_name, conf, model_file.replace('"', ''), int(start_iter), int(end_iter))
        except (KeyboardInterrupt, EOFError) as e:
            break
