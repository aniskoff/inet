from inet.test.fingerprints import *
from inet.test.leak import *
from inet.test.regression import *
from inet.test.run import *
from inet.test.smoke import *
from inet.test.speed import *
from inet.test.validation import *

def run_all_tests(**kwargs):
    run_smoke_tests(**kwargs)
    run_regression_tests(**kwargs)
    run_validation_tests(**kwargs)
    run_leak_tests(**kwargs)
    run_speed_tests(**kwargs)
