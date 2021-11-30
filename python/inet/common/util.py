import os

def get_full_path(resource):
    return os.environ['INET_ROOT'] + "/" + resource
