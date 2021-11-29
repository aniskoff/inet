import json
import os

def get_full_path(resource):
    return os.environ['INET_ROOT'] + "/" + resource

def load_fingerprints(file_name):
    return json.load(open(file_name))

all_fingerprints = load_fingerprints(get_full_path("python/inet/test/fingerprints.json"))
