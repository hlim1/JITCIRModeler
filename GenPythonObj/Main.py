import os, sys
import argparse
import pickle
import json

import Json2PythonObj as json2py

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
    IrJsonFile = argument_parser()
