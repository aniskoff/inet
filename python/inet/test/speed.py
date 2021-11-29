from inet.simulation import *
from inet.test.run import *

def run_speed_tests(**kwargs):
    print("Running speed tests")
    # TODO run_time measurements
    run_tests(test_filter = lambda test: 'run_times' in test, **kwargs)
