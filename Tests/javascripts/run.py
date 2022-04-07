"""
    This script automatically runs all test files in the test directory.

    Author: Terrence Lim.
"""

import copy
import json
import os, sys
import argparse
import subprocess

# Reference: https://www.nsnam.org/wiki/HOWTO_understand_and_find_cause_of_exited_with_code_-11_errors
RETURNCODES = {
        "-1":"SIGHUP",
        "-2":"SIGINT",
        "-3":"SIGQUIT",
        "-4":"SIGILL",
        "-5":"SIGTRAP",
        "-6":"SIGABRT/SIGIOT",
        "-7":"SIGBUS",
        "-8":"SIGFPE",
        "-9":"SIGKILL",
        "-10":"SIGUSR1",
        "-11":"SIGSEGV",
        "-12":"SIGUSR2",
        "-13":"SIGPIPE",
        "-14":"SIGALRM",
        "-15":"SIGTERM",
}

# Command-line arguments to run IR Modeler.
IRMODELER = [
        "../../../pin",
        "-t",
        "../../IRModeler/obj-intel64/IRModeler.so",
        "--"
]

# Path to 'irJsons' directory.
IRJSONS_PATH = "./irJsons"

def run_test_files(arguments: dict):
    """This function runs all test files with the selected JIT compiler
    system executable with IR Modeler to check that the input code has
    no bug.

    args:
        arguments (dict): dictionary holding arguments.

    returns:
        (list) list of files to skip due to error while running.
    """
   
    cmd = copy.copy(arguments["executable-with-args"])
    assert (
            os.path.exists(cmd[0])
    ), f"ERROR: Executable {cmd[0]} does not exists."
    cmd.append(None)
    test_dir = copy.copy(arguments["test-directory"])
    assert (
            os.path.exists(test_dir)
    ), f"ERROR: Test directory does not exists."
    files = os.listdir(test_dir)

    # Collect and hold files to skip when running with IR Modeler.
    skip = []

    print ("Running test files without IR Modeler...")

    for f in files:
        if f.endswith(arguments["language-ext"]):
            cmd[-1] = f"{test_dir}/{f}"
            output = subprocess.run(cmd, capture_output=True, text=True)

            if str(output.returncode) not in RETURNCODES:
                print (f"Test File {f} ... PASSED")
            else:
                print (f"Test File {f} ... FAILED with Return Code {RETURNCODES[str(output.returncode)]}")
                skip.append(f)

    return skip

def run_ir_modeler(arguments: dict, skip: list):
    """This function runs all test files in the test directory with IR Modeler.

    args:
        arguments (dict): dictionary holding arguments.
        skip (list): list of files to skip IR modeling.

    returns:
        None.
    """

    cmd = copy.copy(IRMODELER)
    cmd.extend(copy.copy(arguments["executable-with-args"]))
    cmd.append(None)

    test_dir = arguments["test-directory"]
    files = os.listdir(test_dir)

    print ("Running test files with IR Modeler...")

    for f in files:
        if f.endswith(arguments["language-ext"]) and f not in skip:
            cmd[-1] = f"{test_dir}/{f}"
            output = subprocess.run(cmd, capture_output=True, text=True)
            if str(output.returncode) not in RETURNCODES:
                print (f"Test File {f} ... PASSED")
                file_id = int(f.split('_')[1].split('.')[0])
                file_path = f"{IRJSONS_PATH}/ir_{file_id}.json"
                subprocess.run(['mv', './ir.json', file_path])
            else:
                print (f"Test File {f} ... FAILED with Return Code {RETURNCODES[str(output.returncode)]}")
        elif f in skip:
            print (f"Test file {F} ... SKIPPED")

    os.remove("./trace.out")
    os.remove("./data.out")
    os.remove("./error.out")

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
            help="A file that holds all the command-line arugments to process."
    )
    args = parser.parse_args()

    return args.file

if __name__ == "__main__":
    argument_f = argument_parser()
    arguments = load_json(argument_f)

    # Run all test files in the test directory without IR Modeler.
    skip = run_test_files(arguments)
    
    # Run all test files in the test directory with IR Modeler.
    run_ir_modeler(arguments, skip)
