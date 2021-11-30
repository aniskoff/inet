import csv
import json
import re

from inet.common import *

def load_simulations(file_name):
    simulations = json.load(open(file_name))
    # TODO compatibility
    for simulation in simulations:
        simulation["wd"] = simulation["working_directory"]
        simulation["simtimelimit"] = simulation["sim_time_limit"]
        simulation["args"] = "-f " + simulation["ini_file"] + " -c " + simulation["config"] + " -r " + str(simulation["run"])
    return simulations

def parse_simulations(csv_file):
    def commentRemover(csv_data):
        p = re.compile(' *#.*$')
        for line in csv_data:
            yield p.sub('',line.decode('utf-8'))
    simulations = []
    f = open(csv_file, 'rb')
    csvReader = csv.reader(commentRemover(f), delimiter=str(','), quotechar=str('"'), skipinitialspace=True)
    for fields in csvReader:
        if len(fields) == 0:
            pass        # empty line
        elif len(fields) == 6:
            if fields[4] in ['PASS', 'FAIL', 'ERROR']:
                simulations.append({
                        'file': csv_file,
                        'line' : csvReader.line_num,
                        'wd': fields[0],
                        'args': fields[1],
                        'simtimelimit': fields[2],
                        'fingerprint': fields[3],
                        'expectedResult': fields[4],
                        'tags': fields[5]
                        })
            else:
                raise Exception(csv_file + " Line " + str(csvReader.line_num) + ": the 5th item must contain one of 'PASS', 'FAIL', 'ERROR'" + ": " + '"' + '", "'.join(fields) + '"')
        else:
            raise Exception(csv_file + " Line " + str(csvReader.line_num) + " must contain 6 items, but contains " + str(len(fields)) + ": " + '"' + '", "'.join(fields) + '"')
    f.close()
    return simulations

def read_simulations(csv_file):
    return parse_simulations(csv_file)

def read_examples():
    return read_simulations(get_full_path("tests/fingerprint/examples.csv"))

def read_showcases():
    return read_simulations(get_full_path("tests/fingerprint/showcases.csv"))

def read_tutorials():
    return read_simulations(get_full_path("tests/fingerprint/tutorials.csv"))

global all_examples, all_showcases, all_tutorials, all_simulations, all_tests

all_examples = read_examples()
all_showcases = read_showcases()
all_tutorials = read_tutorials()
all_old_simulations = all_examples + all_showcases + all_tutorials
all_simulations = load_simulations(get_full_path("python/inet/simulation/simulations.json"))
