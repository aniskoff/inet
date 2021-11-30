import functools
import glob
import multiprocessing
import omnetpp
import omnetpp.scave.analysis
import os
import re
import shutil
import subprocess
import sys

from inet.simulation.simulations import *

def ignore_result(result):
    return None

def return_result(result):
    return result

def get_simulations(simulations = None, path_filter = None, args_filter = None, simulation_filter = lambda simulation: True, fullMatch = False, **kwargs):
    global all_old_simulations
    if simulations is None:
        simulations = all_old_simulations
    return list(filter(lambda simulation: (re.search(path_filter if fullMatch else ".*" + path_filter + ".*", simulation['wd']) if path_filter else True) and
                                          (re.search(args_filter if fullMatch else ".*" + args_filter + ".*", simulation['args']) if args_filter else True) and
                                          simulation_filter(simulation),
                       simulations))

def clean_simulation_results(simulation):
    print("Cleaning simulation results, folder = " + simulation['wd'])
    path = get_full_path(simulation['wd']) + "results"
    if not re.search(".*home.*", path):
        raise Exception("Path is not in home")
    if os.path.exists(path):
        shutil.rmtree(path)

def clean_simulations_results(simulations = None, **kwargs):
    if not simulations:
        simulations = get_simulations(**kwargs)
    for simulation in simulations:
        clean_simulation_results(simulation)

def run_simulation(working_directory, sim_time_limit, sim_time_limit_factor = None, mode = "debug", ui = "Cmdenv", ini_file = "omnetpp.ini", config = "General", run = 0, print_end = "\n", extra_args = [], check_result = ignore_result, **kwargs):
    if sim_time_limit_factor is not None:
        sim_time_limit = float(re.sub("(.*)s", "\\1", sim_time_limit))
        sim_time_limit *= sim_time_limit_factor
        sim_time_limit = str(sim_time_limit) + "s"
    print("Running simulation, folder = " + working_directory, end = print_end)
    sys.stdout.flush()
    result = subprocess.run(["inet", "--" + mode, "-s", "-u", ui, "-c", config, "-r", str(run), "--sim-time-limit", sim_time_limit, *extra_args], cwd = "/home/levy/workspace/inet" + working_directory, capture_output = True)
    return check_result(result)

# TODO merge into run_simulation?
def run_simulation2(simulation, sim_time_limit = None, get_extra_args = None, ini_file = None, config = None, run = None, check_result = ignore_result, **kwargs):
    if sim_time_limit is None:
        sim_time_limit = simulation['simtimelimit']
    args = simulation['args'].split()
    if ini_file is None:
        ini_file = args[1]
    if config is None:
        config = args[3]
    if run is None:
        run = args[5]
    extra_args = get_extra_args(simulation, **kwargs) if get_extra_args else []
    result = run_simulation(working_directory = simulation['wd'], sim_time_limit = sim_time_limit, ini_file = ini_file, config = config, run = run, check_result = check_result, extra_args = extra_args, **kwargs)
    return check_result(result)

def run_simulations(simulations = None, run_simulation = run_simulation2, check_result = ignore_result, concurrent = True, **kwargs):
    if simulations is None:
        simulations = get_simulations(**kwargs)
    if len(simulations) > 1:
        print("Running " + str(len(simulations)) + " simulations")
    pool = multiprocessing.Pool(multiprocessing.cpu_count())
    if "check_result" in kwargs:
        del kwargs["check_result"]
    if concurrent:
        partial = functools.partial(run_simulation, **kwargs)
        result = pool.map(partial, simulations)
    else:
        result = list(map(lambda simulation: run_simulation(simulation, **kwargs), simulations))
    return check_result(result)

def get_analysis_files(path_filter = ".*", fullMatch = False):
    analysisFiles = glob.glob("examples/**/*.anf", recursive = True) + \
                    glob.glob("showcases/**/*.anf", recursive = True) + \
                    glob.glob("tutorials/**/*.anf", recursive = True)
    return filter(lambda path: re.search(path_filter if fullMatch else ".*" + path_filter + ".*", path), analysisFiles)

def export_charts(**kwargs):
    for analysisFile in get_analysis_files(**kwargs):
        print("Exporting charts, analysisFile = " + analysisFile)
        analysis = omnetpp.scave.analysis.load_anf_file(analysisFile)
        for chart in analysis.charts:
            folder = os.path.dirname(analysisFile)
            analysis.export_image(chart, get_full_path(folder), workspace, format="png", dpi=150, target_folder="doc/media")

def generate_charts(**kwargs):
    clean_simulations_results(**kwargs)
    run_simulations(**kwargs)
    export_charts(**kwargs)
