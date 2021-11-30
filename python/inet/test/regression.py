import re

from omnetpp.scave.results import *
from inet.simulation.run import *
from inet.test.fingerprints import *
from inet.test.run import *

def fingerprint_check(result, fingerprint = None, **kwargs):
    stdout = result.stdout.decode("utf-8")
    old_fingerprint = fingerprint
    if re.search("Fingerprint successfully verified", stdout):
        new_fingerprint = old_fingerprint
    else:
        new_fingerprint = re.search("Fingerprint mismatch! calculated: (.*?),", stdout).groups()[0]
    return old_fingerprint == new_fingerprint

def get_fingerprint(simulation, ingredients = "tplx", sim_time_limit_factor = 1.0):
    global all_fingerprints
    def f(fingerprint):
        return fingerprint["ingredients"] == ingredients and fingerprint["sim_time_limit_factor"] == sim_time_limit_factor
    for fingerprints in all_fingerprints:
        if fingerprints["uuid"] == simulation["uuid"]:
            result = [e["fingerprint"] for e in filter(f, fingerprints["fingerprints"])]
            if result:
                return result[0]
    return None

def get_regression_test_extra_args(simulation, sim_time_limit_factor = "1.0", **kwargs):
    fingerprint = get_fingerprint(simulation, "tplx", sim_time_limit_factor) or "0000-0000/tplx"
    return ["--fingerprintcalculator-class", "inet::FingerprintCalculator", "--fingerprint", fingerprint]

def run_regression_tests(**kwargs):
    print("Running regression tests")
    # TODO fingerprint comparison
    run_tests(get_extra_args = get_regression_test_extra_args, test_check = fingerprint_check, **kwargs)

def update_fingerprints():
    # TODO
    return
