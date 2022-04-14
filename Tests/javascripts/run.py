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

# Path to 'irJsons' directory.
V8_IRJSONS_PATH = "./v8IRJsons"
JSC_IRJSONS_PATH = "./jscIRJsons"
SPM_IRJSONS_PATH = "./spmIRJsons"

def run_test_files(arguments: dict):
    """This function runs all test files with the selected JIT compiler
    system executable with IR Modeler to check that the input code has
    no bug.

    args:
        arguments (dict): dictionary holding arguments.

    returns:
        (list) list of files to skip due to error while running with V8.
        (list) list of files to skip due to error while running with JSC.
        (list) list of files to skip due to error while running with SPM.
    """

    print ("Running Test Files without IR Modeler...")
    
    # Extract Test Files Directory Path.
    test_dir = copy.copy(arguments["test-directory"])
    assert (
            os.path.exists(test_dir)
    ), f"ERROR: Test directory does not exists."
    files = os.listdir(test_dir)

    # Extract Command-line Arguments of Each JIT Compiler System.
    v8_cmd = copy.copy(arguments["v8-executable-with-args"])
    jsc_cmd = copy.copy(arguments["jsc-executable-with-args"])
    spm_cmd = copy.copy(arguments["spm-executable-with-args"])

    # Preapre Empty Skip Holders.
    v8Skip = []
    jscSkip = []
    spmSkip = []

    # Run Test Files with V8 Executable (+arguments).
    if v8_cmd:
        print ("Test for V8 ... BEGIN")
        v8_cmd.append(None)
        v8Skip = run_tests(arguments, v8_cmd, files)
    else:
        print ("Test for V8 ... SKIP")
    # Run Test Files with SpiderMonkey Executable (+arguments).
    if spm_cmd:
        print ("Test for SpiderMonkey ... BEGIN")
        spm_cmd.append(None)
        spmSkip = run_tests(arguments, spm_cmd, files)
    else:
        print ("Test for SpiderMonkey ... SKIP")
    # Run Test Files with JavaScriptCore Executable (+arguments).
    if jsc_cmd:
        print ("Test for JavaScriptCore ... BEGIN")
        jsc_cmd.append(None)
        jscSkip = run_tests(arguments, jsc_cmd, files)
    else:
        print ("Test for JavaScriptCore ... SKIP")

    return v8Skip, jscSkip, spmSkip

def run_tests(arguments: dict, cmd: list, files: list):
    """This function runs all test files in the test directory with the passed
    command-line argument.

    args:
        arguments (dict): dictionary holding arguments.
        cmd (list): command-line to run the test files.
        files (list): list of all file names in the test directory.

    returns:
        (list) list of files to skip due to error while running.
    """

    # Extract Test Files Directory Path.
    test_dir = copy.copy(arguments["test-directory"])

    # Collect and hold files to skip when running with IR Modeler.
    skip = []

    for f in files:
        if f.endswith(arguments["language-ext"]):
            cmd[-1] = f"{test_dir}/{f}"
            output = subprocess.run(cmd, capture_output=True, text=True)

            if str(output.returncode) not in RETURNCODES:
                print (f"   Test File {f} ... PASSED")
            else:
                print (f"   Test File {f} ... FAILED with Return Code {RETURNCODES[str(output.returncode)]}")
                skip.append(f)

    return skip

def run_ir_modeler(arguments: dict, v8Skip: list, jscSkip: list, spmSkip: list):
    """This function runs all test files in the test directory with IR Modeler.

    args:
        arguments (dict): dictionary holding arguments.
        skip (list): list of files to skip IR modeling.

    returns:
        None.
    """

    print ("Running test files with IR Modeler...")

    # Extract Test Files Directory Path.
    test_dir = copy.copy(arguments["test-directory"])
    files = os.listdir(test_dir)

    # Extract Command-line Arguments of Each JIT Compiler System.
    v8_cmd = copy.copy(arguments["v8-executable-with-args"])
    jsc_cmd = copy.copy(arguments["jsc-executable-with-args"])
    spm_cmd = copy.copy(arguments["spm-executable-with-args"])

    # Model IR of V8 JIT Compiler.
    if v8_cmd:
        print ("IR Modeling for V8 ... BEGIN")
        cmd = get_ir_modeler_cmd(arguments, v8_cmd)
        get_ir_model(arguments, cmd, files, v8Skip, V8_IRJSONS_PATH)
    else:
        print ("IR Modeling for V8 ... SKIP")
    # Model IR of SpiderMonkey JIT Compiler.
    if spm_cmd:
        print ("IR Modeling for SpiderMonkey ... BEGIN")
        cmd = get_ir_modeler_cmd(arguments, spm_cmd)
        get_ir_model(arguments, cmd, files, spmSkip, SPM_IRJSONS_PATH)
    else:
        print ("IR Modeling for SpiderMonkey ... SKIP")
    # Model IR of JavaScriptCore JIT Compiler.
    if jsc_cmd:
        print ("IR Modeling for JavaScriptCore ... BEGIN")
        cmd = get_ir_modeler_cmd(arguments, jsc_cmd)
        get_ir_model(arguments, cmd, files, jscSkip, JSC_IRJSONS_PATH)
    else:
        print ("IR Modeling for JavaScriptCore ... SKIP")

    # Remove Unnecessary .out files.
    os.remove("./trace.out")
    os.remove("./data.out")
    os.remove("./errors.out")

def get_ir_modeler_cmd(arguments: dict, sys_cmd: list):
    """This function constructs the command-line to pass to subprocess.run
    with the Pin Tool, IRModeler, and JIT compiler system execution (+args).

    args:
        arguments (dict): dictionary holding arguments.
        sys_cmd (list): JIT compiler system command-line (+arguments).

    returns:
        (list) constructed command-line.
    """

    # Prepare Command-line for Pin Tool and IRModeler.
    cmd = [
            arguments["pin-path"],
            "-t",
            arguments["irmodeler-path"],
            "--"
    ]

    cmd.extend(sys_cmd)
    cmd.append(None)

    return cmd

def get_ir_model(arguments: dict, cmd: list, files: list, skip: list, IRJsons_Path: str):
    """This function runs IRModeler and JIT compuler system executable to
    get the modeled IR.

    args:
        arguments (dict): dictionary holding arguments.
        cmd (list): command-line list to pass to subprocess.run.
        files (list): list of all files in the test directory.
        skip (list): list of files to skip IR modeling.
        IRJsons_Path (str): path to where the generated IRs to be stored.

    returns:
        None.
    """

    test_dir = copy.copy(arguments["test-directory"])

    for f in files:
        if f.endswith(arguments["language-ext"]) and f not in skip:
            cmd[-1] = f"{test_dir}/{f}"
            output = subprocess.run(cmd, capture_output=True, text=True)
            if str(output.returncode) not in RETURNCODES:
                print (f"   Test File {f} ... PASSED")
                file_id = int(f.split('_')[1].split('.')[0])
                file_path = f"{IRJsons_Path}/ir_{file_id}.json"
                subprocess.run(['mv', './ir.json', file_path])
            else:
                print (f"   Test File {f} ... FAILED with Return Code {RETURNCODES[str(output.returncode)]}")
        elif f in skip:
            print (f"   Test file {F} ... SKIPPED")


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
    v8Skip, jscSkip, spmSkip = run_test_files(arguments)

    # Run all test files in the test directory with IR Modeler.
    run_ir_modeler(arguments, v8Skip, jscSkip, spmSkip)
