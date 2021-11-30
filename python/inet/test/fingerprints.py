import json
import os

from inet.common import *

def load_fingerprints(file_name):
    return json.load(open(file_name))

all_fingerprints = load_fingerprints(get_full_path("python/inet/test/fingerprints.json"))
