import itertools
import numpy
import random

import inet.test.run

from omnetpp.scave.results import *
from inet.test.run import *

def run_validation_tsn_framereplication_simulation(**kwargs):
    return run_simulation(working_directory = "/validation/tsn/framereplication/", sim_time_limit = "0.1s", print_end = " ", **kwargs)

def compute_frame_replication_success_rate_with_simulation(**kwargs):
    run_validation_tsn_framereplication_simulation(**kwargs)
    filter_expression = """type =~ scalar AND ((module =~ "*.destination.udp" AND name =~ packetReceived:count) OR (module =~ "*.source.udp" AND name =~ packetSent:count))"""
    df = read_result_files("validation/tsn/framereplication/results/*.sca", filter_expression = filter_expression)
    df = get_scalars(df)
    packetSent = float(df[df.name == "packetSent:count"].value)
    packetReceived = float(df[df.name == "packetReceived:count"].value)
    return packetReceived / packetSent

def compute_frame_replication_success_rate_analytically1():
    combinations = numpy.array(list(itertools.product([0, 1], repeat=7)))
    probabilities = numpy.array([0.8, 0.8, 0.64, 0.8, 0.64, 0.8, 0.8])
    solutions = numpy.array([[1, 1, 1, 0, 0, 0, 0], [1, 1, 0, 0, 1, 1, 0], [1, 0, 0, 1, 1, 0, 0,], [1, 0, 1, 1, 0, 0, 1]])
    p = 0
    for combination in combinations:
        probability = (combination * probabilities + (1 - combination) * (1 - probabilities)).prod()
        for solution in solutions:
            if (solution * combination == solution).all() :
                p += probability
                break   
    return p

def compute_frame_replication_success_rate_analytically2():
    successful = 0
    n = 1000000
    for i in range(n):
        s1 = random.random() < 0.8
        s1_s2a = random.random() < 0.8
        s1_s2b = random.random() < 0.8
        s2a_s2b = random.random() < 0.8
        s2b_s2a = random.random() < 0.8
        s2a = (s1 and s1_s2a) or (s1 and s1_s2b and s2b_s2a)
        s2b = (s1 and s1_s2b) or (s1 and s1_s2a and s2a_s2b)
        s3a = s2a and (random.random() < 0.8)
        s3b = s2b and (random.random() < 0.8)
        s3a_s4 = random.random() < 0.8
        s3b_s4 = random.random() < 0.8
        s4 = (s3a and s3a_s4) or (s3b and s3b_s4)
        if s4:
            successful += 1
    return successful / n

def run_validation_tsn_framereplication_test(test_accuracy = 0.01, **kwargs):
    ps = compute_frame_replication_success_rate_with_simulation(**kwargs)
    pa1 = compute_frame_replication_success_rate_analytically1()
    pa2 = compute_frame_replication_success_rate_analytically2()
    test_result1 = abs(ps - pa1) / pa1 < test_accuracy
    test_result2 = abs(ps - pa2) / pa2 < test_accuracy
    print_test_result(test_result1 and test_result2)

def run_validation_tsn_trafficshaping_asynchronousshaper_simulation(**kwargs):
    run_simulation(working_directory = "/validation/tsn/trafficshaping/asynchronousshaper", sim_time_limit = "10s", print_end = " ", **kwargs)

def compute_asynchronousshaper_todo_with_simulation(**kwargs):
    run_validation_tsn_trafficshaping_asynchronousshaper_simulation(**kwargs)
    return 0

def compute_asynchronousshaper_todo_analytically():
    return 0

def run_validation_tsn_trafficshaping_asynchronousshaper_test(**kwargs):
    v1 = compute_asynchronousshaper_todo_with_simulation(**kwargs)
    v2 = compute_asynchronousshaper_todo_analytically()
    test_result = v1 == v2
    print_test_result(test_result)

def run_validation_tsn_trafficshaping_creditbasedshaper_simulation(**kwargs):
    return run_simulation(working_directory = "/validation/tsn/trafficshaping/creditbasedshaper", sim_time_limit = "1s", print_end = " ", **kwargs)

def compute_creditbasedshaper_todo_with_simulation(**kwargs):
    run_validation_tsn_trafficshaping_creditbasedshaper_simulation(**kwargs)
    return 0

def compute_creditbasedshaper_todo_analytically(**kwargs):
    return 0

def run_validation_tsn_trafficshaping_creditbasedshaper_test(**kwargs):
    v1 = compute_creditbasedshaper_todo_with_simulation(**kwargs)
    v2 = compute_creditbasedshaper_todo_analytically(**kwargs)
    test_result = v1 == v2
    print_test_result(test_result)

def run_validation_tests(**kwargs):
    print("Running validation tests")
    run_validation_tsn_framereplication_test(**kwargs)
    run_validation_tsn_trafficshaping_asynchronousshaper_test(**kwargs)
    run_validation_tsn_trafficshaping_creditbasedshaper_test(**kwargs)
