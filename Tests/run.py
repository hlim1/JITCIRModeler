"""
"""

import json
import os, sys

JSEXT = ".js"

# Paths
ARGUMENT_FILE_PATH = "./arguments.json"
JSCODES_PATH = "./codes"
V8_PATHS = {
        "8.3.1": "./v8/v8_8.3.1",
}

# V8
V8_VERSIONS = [
        "8.3.1"
]
V8_ARGUMENTS = [
        "--no-turbo-inlining",
        "--single-threaded"
]


def load_json(json_file: str):
    try:
        with open(json_file) as f:
            return json.load(f)
    except IOError as x:
        assert False, f"{json_file} cannot be opened."

if __name__ == "__main__":
    arguments = load_json(ARGUMENT_FILE_PATH)

