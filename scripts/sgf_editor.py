#!/usr/bin/python

import os
import sys
import argparse
from argparse import RawTextHelpFormatter
import numpy as np
import re
from tqdm import tqdm


class string_editor:

    def __init__(self, regex: str):
        self.regex = re.compile(regex)

    def insert_before(self, dest: str, s: str):
        return self.regex.sub(s + r'\g<0>', dest)

    def insert_after(self, dest: str, s: str):
        return self.regex.sub(r'\g<0>' + s, dest)

    def replace(self, dest: str, s: str):
        return self.regex.sub(s, dest)


class utils:

    pos_mapping = {
        'a': 1,
        'b': 2,
        'c': 3,
        'd': 4,
        'e': 5,
        'f': 6,
        'g': 7,
        'h': 8,
        'i': 9,
        'j': 10,
        'k': 11,
        'l': 12,
        'm': 13,
        'n': 14,
        'o': 15,
        'p': 16,
        'q': 17,
        'r': 18,
        's': 19,
        't': 20,
        'u': 21,
        'v': 22,
        'w': 23,
        'x': 24,
        'y': 25,
        'z': 26,
        1: 'a',
        2: 'b',
        3: 'c',
        4: 'd',
        5: 'e',
        6: 'f',
        7: 'g',
        8: 'h',
        9: 'i',
        10: 'j',
        11: 'k',
        12: 'l',
        13: 'm',
        14: 'n',
        15: 'o',
        16: 'p',
        17: 'q',
        18: 'r',
        19: 's',
        20: 't',
        21: 'u',
        22: 'v',
        23: 'w',
        24: 'x',
        25: 'y',
        26: 'z'
    }

    @staticmethod
    def transform_pos(pos: str, size: int, opcode: int):
        pos = utils.pos_mapping[pos[0]], utils.pos_mapping[pos[1]]
        if opcode == 0:
            return utils.pos_mapping[pos[0]] + utils.pos_mapping[pos[1]]
        elif opcode == 1:
            return utils.pos_mapping[size - pos[0] + 1] + utils.pos_mapping[pos[1]]
        elif opcode == 2:
            return utils.pos_mapping[pos[0]] + utils.pos_mapping[size - pos[1] + 1]
        elif opcode == 3:
            return utils.pos_mapping[size - pos[1] + 1] + utils.pos_mapping[pos[0]]
        elif opcode == 4:
            return utils.pos_mapping[size - pos[0] + 1] + utils.pos_mapping[size - pos[1] + 1]
        elif opcode == 5:
            return utils.pos_mapping[pos[1]] + utils.pos_mapping[size - pos[0] + 1]
        else:
            raise argparse.ArgumentTypeError(f"Unknown opcode. ({opcode})")

    @staticmethod
    def transform_board(s: str, size: int, opcode: int):
        board, pass_move = utils.decode_board(s, size)
        if opcode == 0:
            pass
        elif opcode == 1:
            board = board[:, ::-1]
        elif opcode == 2:
            board = board[::-1, :]
        elif opcode == 3:
            board = np.rot90(board, -1)
        elif opcode == 4:
            board = np.rot90(board, 2)
        elif opcode == 5:
            board = np.rot90(board, 1)
        else:
            raise argparse.ArgumentTypeError(f"Unknown opcode. ({opcode})")
        return utils.encode_board(board, pass_move)

    @staticmethod
    def binary(num, length=-1, base=16):
        return bin(int(num, base=base))[2:].zfill(length)

    @staticmethod
    def split(s: str, n: int):
        return [s[i:i + n] for i in range(0, len(s), n)]

    @staticmethod
    def decode_board(s: str, size: int):
        num_pos = size * size
        bin_str = utils.binary(s, num_pos)
        pass_move = len(bin_str) == num_pos + 1
        board = np.array(list(bin_str[-num_pos:]), dtype=int).reshape((size, size))
        return (board[:, ::-1], pass_move)

    @staticmethod
    def encode_board(board: np.array, pass_move: bool):
        bin_str = ''.join(map(str, board[:, ::-1].ravel()))
        if pass_move:
            bin_str = '1' + bin_str
        return hex(int(bin_str, base=2))[2:]


class node_parser:

    def __init__(self, mapping: map):
        self.mapping = mapping.copy()
        self.mapping_list = [(re.compile(key), mapping[key]) for key in mapping]

    def __repr__(self):
        return f'node_parser({repr(self.mapping)})'

    def parse(self, s: str):
        for matcher in self.mapping_list:
            match = matcher[0].search(s)
            if match is None:
                continue
            start, end = match.start(1), match.end(1)
            replace = matcher[1](match.group(1))
            s = ''.join((s[:start], replace, s[end:]))
        return s


if __name__ == '__main__':
    script_file_name = re.split(r'[/\\]', __file__)[-1]

    parser = argparse.ArgumentParser(formatter_class=RawTextHelpFormatter, epilog=f'\
example: clockwise rotate 90 and then horizontally flip\n\n\
    > {script_file_name} sgf_path -t 20\n ')
    parser.add_argument('sgf_path', type=str, help='Input file name')
    parser.add_argument('-o', '--output', help='Set output file name', default='')
    parser.add_argument('-t', '--transform', help=f'\
no transformation   : 0\n\
horizontally flip   : 1\n\
vertically flip     : 2\n\
clockwise rotate 90 : 3\n\
clockwise rotate 180: 4\n\
clockwise rotate 270: 5\n\
horizontally flip + clockwise rotate 90 : 13\n\
horizontally flip + clockwise rotate 270: 15', default='')
    parser.add_argument('-rz', '--rzone', action='store_true', help='Convert RZone from old to new format')
    args = parser.parse_args()

    if not args.sgf_path.endswith('.sgf'):
        raise Exception('Not a sgf file.')

    with open(args.sgf_path) as f:
        sgf_string = f.read()

    match = re.search('SZ\\[([1-9][0-9]*)\\]', sgf_string)
    if match is None:
        raise Exception("Sgf format error. (Can't find SZ[size].)")

    board_size = int(match.group(1))
    opcodes = list(map(int, args.transform))

    def process_pos(s: str):
        if not s:
            return s
        for opcode in opcodes:
            s = utils.transform_pos(s, board_size, opcode)
        return s

    def process_board(s: str):
        if not s:
            return s
        for opcode in opcodes:
            s = utils.transform_board(s, board_size, opcode)
        return s

    p = node_parser({
        r';[WBwb]\[([^\r\n\[\]]*)\]': (lambda s: process_pos(s)),
        r'RZ\[([0-9A-Za-z]*)\]': (lambda s: process_board(s)),
        r'RZ\[[0-9A-Za-z]*\]\[([0-9A-Za-z]*)\]': (lambda s: process_board(s)),
        r'RZ\[[0-9A-Za-z]*\]\[[0-9A-Za-z]*\]\[([0-9A-Za-z]*)\]': (lambda s: process_board(s)),
        # r'solver_status: ([^\r\n\[\]]*)': (lambda s: s),
        # r'p = ([^\r\n\[\]]*)': (lambda s: s),
        # r'p_logit = ([^\r\n\[\]]*)': (lambda s: s),
        # r'p_noise = ([^\r\n\[\]]*)': (lambda s: s),
        # r'v = ([^\r\n\[\]]*)': (lambda s: s),
        # r'mean = ([^\r\n\[\]]*)': (lambda s: s),
        # r'count = ([^\r\n\[\]]*)': (lambda s: s),
        r'equal_loss = ([^\r\n\[\]]*)': (lambda s: s if s == '-1' else process_pos(s)),
        # r'match_tt = ([^\r\n\[\]]*)': (lambda s: s),
        # r'check_ghi = ([^\r\n\[\]]*)': (lambda s: s),
        # r'rzone_data_index = ([^\r\n\[\]]*)': (lambda s: s),
        # r'ghi_data_index = ([^\r\n\[\]]*)': (lambda s: s),
        r'R: ([^\r\n\[\]]*)': (lambda s: process_board(s)),
        r'G: ([^\r\n\[\]]*)': (lambda s: process_board(s)),
        r'B: ([^\r\n\[\]]*)': (lambda s: process_board(s)),
    })

    rgb_remover = node_parser({
        r'(R: [^\r\n\[\]]*[\r\n]*)': (lambda s: ''),
        r'(G: [^\r\n\[\]]*[\r\n]*)': (lambda s: ''),
        r'(B: [^\r\n\[\]]*[\r\n]*)': (lambda s: ''),
    })

    se = string_editor(r';[WBwb]\[([^\r\n\[\]]*)\]')
    r_matcher = re.compile(r'R: ([^\r\n\[\]]*)')
    g_matcher = re.compile(r'G: ([^\r\n\[\]]*)')
    b_matcher = re.compile(r'B: ([^\r\n\[\]]*)')

    def convert_rzone(s: str):
        r_match = r_matcher.search(s)
        g_match = g_matcher.search(s)
        b_match = b_matcher.search(s)
        if r_match is None and g_match is None and b_match is None:
            return s
        rzone_str = (f'RZ'
                     f'[{r_match.group(1) if r_match != None else 0}]'
                     f'[{g_match.group(1) if g_match != None else 0}]'
                     f'[{b_match.group(1) if b_match != None else 0}]')
        return se.insert_after(s, rzone_str)

    def process_node(s: str):
        if args.rzone:
            s = convert_rzone(s)
            s = rgb_remover.parse(s)
        s = p.parse(s)
        return s

    total = 0
    # count number of nodes
    for match in re.finditer(r';', sgf_string):
        total += 1
    nodes = []
    try:
        # processing each node
        for index, match in tqdm(enumerate(re.finditer(r'\(?;[^;]*', sgf_string)), total=total):
            nodes.append(process_node(match.group()))
    except KeyboardInterrupt as e:
        print('KeyboardInterrupt')
        exit()
    except argparse.ArgumentTypeError as e:
        print(e)
    except Exception:
        raise Exception(f'Node index: {index} (start from 0)\n' + match.group())

    transformed_sgf_string = ''.join(nodes)

    # save to file
    output_path = args.output
    if not output_path:
        filename, file_extension = os.path.splitext(args.sgf_path)
        output_path = filename + '_transformed.sgf'
    if not output_path.endswith('.sgf'):
        output_path += '.sgf'

    with open(output_path, "w") as f:
        f.write(transformed_sgf_string)
