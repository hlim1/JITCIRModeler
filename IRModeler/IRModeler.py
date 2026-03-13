import os
import sys
import json

# Build the path to the IRModeler.so file
script_dir = os.path.dirname(os.path.abspath(__file__))
irmodeler_so_path = os.path.join(script_dir, "obj-intel64/IRModeler.so")

# Set opcode table path
opcode_table = "v8_8.3.110.13.json" # Update this to correct opcode table file.
opcode_table_path = os.path.join(script_dir, "opcodeTables", opcode_table)

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
    assert os.path.exists("ir.json"), f"ERROR: ir.json not available."
    opcode2name = load_json(opcode_table_path)

    for node in ir["nodes"]:
        opcode = node["opcode"]
        assert opcode in opcode2name, f"ERROR: {opcode} not in opcode table."
        name = opcode2name[opcode]
        node["mnemonic"] = name
        
        # In addition to adding mnemonic, we sort the directValues by keys (increasing).
        if node["directValues"]:
            node["directValues"] = dict(
                sorted(node["directValues"].items(), key=lambda x: int(x[0]))
            )

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

    addMnemonic()
    clean()

if __name__ == "__main__":
    main()
