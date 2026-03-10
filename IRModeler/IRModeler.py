import os
import sys
import time
import json
import subprocess

# Get PIN_ROOT from environment
pin_root = os.environ.get("PIN_ROOT")

# Build the path to the pin binary
pin_path = os.path.join(pin_root, "pin")

# Build the path to the IRModeler.so file
script_dir = os.path.dirname(os.path.abspath(__file__))
irmodeler_so_path = os.path.join(script_dir, "obj-intel64/IRModeler.so")

# Set opcode table path
opcode_table = "v8_8.3.110.13.json" # Update this to correct opcode table file.
opcode_table_path = os.path.join(script_dir, "opcodeTables", opcode_table)

def confirm_paths():
    """
    Verify that all required paths exist before execution.

    This function checks that critical files and directories required by the
    workflow are available on the filesystem. These include:

        - PIN installation root directory
        - PIN executable path
        - IRModeler shared library (.so)
        - Opcode table JSON file

    If any path is missing, the function terminates execution with an
    assertion error.

    Raises
    ------
    AssertionError
        If any required file or directory does not exist.
    """
    assert os.path.exists(pin_root), f"ERROR: {pin_root} not available."
    assert os.path.exists(pin_path), f"ERROR: {pin_path} not available."
    assert os.path.exists(irmodeler_so_path), f"ERROR: {irmodeler_so_path} not available."
    assert os.path.exists(opcode_table_path), f"ERROR: {opcode_table_path} not available."

def run(cmd: str):
    """
    Execute a shell command and report its execution results.

    This function runs a command using the subprocess module, captures its
    standard output and standard error streams, and prints them to the console.
    It also measures and reports the execution time.

    Parameters
    ----------
    cmd : str
        Command string to execute.

    Behavior
    --------
    - Executes the command using subprocess.run()
    - Captures stdout and stderr
    - Prints execution output
    - Prints elapsed execution time

    Returns
    -------
    None
    """
    start = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True)
    end = time.time()

    # Execution result.
    print("STDOUT:")
    print(result.stdout)
    print("STDERR:")
    print(result.stderr)

    elapsed = end - start
    minutes = int(elapsed // 60)
    seconds = elapsed % 60
    print("Elapsed time: {} min {:.0f} sec".format(minutes, seconds))

def clean(files=["data.out", "errors.out"]):
    """
    Remove generated output files.

    This function deletes temporary or generated files produced during the
    workflow. It first verifies that each file exists before attempting to
    remove it.

    Parameters
    ----------
    files : list of str, optional
        List of filenames to remove. Default:
            ["data.out", "errors.out"]

    Raises
    ------
    AssertionError
        If any specified file does not exist.

    Side Effects
    ------------
    Deletes files from the filesystem.
    """
    for f in files:
        assert os.path.exists(f), f"ERROR: {f} not available."
        os.remove(f)

def addMnemonic():
    """
    Add opcode mnemonic names to nodes in the IR JSON file.

    This function reads the IR dataset from `ir.json` and an opcode lookup
    table from the opcode table file. For each node, it resolves the opcode
    value to its corresponding mnemonic name and stores it in a new field
    called `mnemonic`.

    Workflow
    --------
    1. Load IR data from `ir.json`
    2. Load opcode-to-name mapping from opcode table
    3. For each node:
        - Look up its opcode
        - Attach the corresponding mnemonic
    4. Write the updated JSON back to `ir.json`

    Raises
    ------
    AssertionError
        If a node's opcode does not exist in the opcode table.

    Returns
    -------
    None
    """
    ir = load_json("ir.json")
    opcode2name = load_json(opcode_table_path)

    for node in ir["nodes"]:
        opcode = node["opcode"]
        assert opcode in opcode2name, f"ERROR: {opcode} not in opcode table."
        name = opcode2name[opcode]
        node["mnemonic"] = name

    dump_json(ir, "ir.json")

def load_json(path: str):
    """
    Load and parse a JSON file.

    Parameters
    ----------
    path : str
        Path to the JSON file.

    Returns
    -------
    dict
        Parsed JSON object.
    """
    assert os.path.exists(path), f"ERROR: {path} not available."

    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def dump_json(data: list, path: str, space=4):
    """
    Write JSON data to a file with formatting.

    This function serializes a Python object to JSON format and writes it
    to the specified file. The output is formatted with indentation to
    improve readability.

    Parameters
    ----------
    data : list or dict
        Data structure to serialize to JSON.
    path : str
        Output file path.
    space : int, optional
        Number of spaces used for indentation (default = 4).

    Raises
    ------
    AssertionError
        If the target file does not exist.

    Side Effects
    ------------
    Overwrites the contents of the specified JSON file.
    """
    assert os.path.exists(path), f"ERROR: {path} not available."

    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=space)

def main():
    confirm_paths()

    args = sys.argv[1:]

    cmd = [pin_path, "-t", irmodeler_so_path, "--"]
    cmd.extend(args)

    run(cmd)
    clean()
    addMnemonic()

if __name__ == "__main__":
    main()
