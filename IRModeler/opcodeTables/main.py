import os
import sys
import json

def load_json(path):
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

def get_name(opcode: str, op2name: dict):
    """
    Retrieve the mnemonic name corresponding to an opcode.

    This function looks up the given opcode in an opcode-to-name mapping
    table and returns the associated mnemonic name.

    Parameters
    ----------
    opcode : str
        Opcode value represented as a string. This value is typically read
        from the IR JSON node field `opcode`.

    op2name : dict
        Dictionary mapping opcode values to their corresponding mnemonic
        names.

    Returns
    -------
    str
        Mnemonic name corresponding to the given opcode.

    Raises
    ------
    AssertionError
        If the opcode does not exist in the opcode lookup table.
    """
    assert opcode in op2name, f"ERROR: Opcode {opcode} is not in the opcode table."

    return op2name[opcode]

def main():
    args = sys.argv[1:]

    file = args[0]
    opcode = args[1]

    op2name = load_json(file)
    name = get_name(opcode, op2name)

    print (f"0x{opcode} = {name}")

if __name__ == "__main__":
    main()
