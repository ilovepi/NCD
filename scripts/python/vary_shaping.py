#! /bin/env python

import subprocess

if __name__ == "__main__":
    large_num_packets = range(500, 8000, 500)
    large_packet_sizes = [512, 1024, 1400]
    small_num_packets = range(4000, 20000, 1000)
    small_packet_sizes = [32, 64, 128, 256]

    args = []
    for size in large_num_packets:
        pack_arg = "--num_packets="+ str(size)
        for length in large_packet_sizes:
            len_arg = "--data_length=" + str(length)
            full_args = "--dport_disc=33333 " + len_arg + " " + pack_arg
            args.append(full_args)

    for size in small_num_packets:
        pack_arg = "--num_packets="+ str(size)
        for length in small_packet_sizes:
            len_arg = "--data_length=" + str(length)
            full_args = "--dport_disc=33333 " + len_arg + " " + pack_arg
            args.append(full_args)

    for parameters in args:
        subprocess.call("./ExperimentSQL.py 'Shaping Parameters Test V1' " + parameters, shell=True)