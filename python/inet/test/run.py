import inet.simulation.simulations

from inet.simulation.run import *
from inet.simulation.simulations import *

def print_test_result(result):
    txtPASS = "\033[0;32m" + "PASS" + "\033[0;0m" # GREEN
    txtPASS_unexpected = "\033[0;32m" + "PASS" + "\033[0;0m" + " " + "\033[1;31m" + "(unexpected)" + "\033[0;0m" # GREEN + RED
    txtFAIL = "\033[1;33m" + "FAIL" + "\033[0;0m" # YELLOW
    txtFAIL_expected = "\033[2;32m" + "FAIL (expected)" + "\033[0;0m" # DARK GREEN
    txtERROR = "\033[1;31m" + "ERROR" + "\033[0;0m" # RED
    txtERROR_expected = "\033[2;32m" + "ERROR (expected)" + "\033[0;0m" # DARK GREEN
    print(txtPASS if result else txtFAIL)

def get_tests(simulations = None, test_filter = lambda test: True, **kwargs):
    global all_simulations
    tests = get_simulations(all_simulations, simulation_filter = test_filter, **kwargs)
    return tests

def check_test_result(result):
    return result.returncode == 0

def run_test(test_check = check_test_result, **kwargs):
    result = run_simulation(print_end = " ", **kwargs)
    test_result = test_check(result, **kwargs)
    print_test_result(test_result)

def run_test2(simulation, test_check = check_test_result, **kwargs):
    result = run_simulation2(simulation, print_end = " ", **kwargs)
    test_result = test_check(result, **kwargs)
    print_test_result(test_result)

def run_tests(**kwargs):
    tests = get_tests(**kwargs)
    if "test_filter" in kwargs:
        del kwargs["test_filter"]
    run_simulations(tests, run_simulation = run_test2, **kwargs)
