import os, sys
import argparse
import pickle
import json

import Json2PythonObj as json2py

def load_json(json_file: str):
    try:
        with open(json_file) as f:
            return json.load(f)
    except IOError as x:
        assert False, f"{json_file} cannot be opened."

def argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
            "-f",
            "--file",
            type=str,
            required=True,
            help="IRModeler generated IR JSON file, ir.json."
    )
    args = parser.parse_args()

    return args.file

if __name__ == "__main__":
    irJsonFile = argument_parser()
    irNodes = load_json(irJsonFile)

    # All IRs generated with this Main.py will have the default IR ID.
    irId = 0

    json2py.Json2PythonObj(irNodes, irId)
