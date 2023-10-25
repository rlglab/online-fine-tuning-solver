#!/usr/bin/python

import sys


def main(argv):
    threshold = int(argv[1])
    entries = []
    input()
    input()
    while True:
        try:
            line = input()
        except BaseException:
            break
        tokens = line.split(' ')
        if int(tokens[-2]) > threshold:
            entries.append((int(tokens[0]), tokens[-2]))
        input()

    sorted_entries = sorted(entries)
    for entry in sorted_entries:
        # print(f'{entry[0]} {entry[1]}')
        print(f'{entry[1]}')


if __name__ == "__main__":
    main(sys.argv)
