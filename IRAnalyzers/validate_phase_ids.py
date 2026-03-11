#!/usr/bin/env python3
"""
validate_phase_ids.py

Author: Terrence Lim (with the help of LLM).
Created: 2026-03-09

Description
-----------
This script validates that all `PhaseFnId` values recorded in the `instAccess`
logs of nodes are valid phase function identifiers.

A PhaseFnId is considered valid if it appears in the `fnId2Name` mapping
contained in the JSON file.

The script scans all nodes and all `instAccess` entries and verifies that
each PhaseFnId exists in `fnId2Name`.

If any invalid PhaseFnId is found, the script prints an error report and
exits with a non-zero status.

Usage
-----
python3 validate_phase_ids.py ir.json

"""

import json
import sys


def load_json(path):
    """
    Load and parse the JSON file.

    Parameters
    ----------
    path : str
        Path to the JSON file.

    Returns
    -------
    dict
        Parsed JSON object.
    """
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def validate_phase_ids(data):
    """
    Validate that all PhaseFnId values in instAccess are valid.

    Parameters
    ----------
    data : dict
        Parsed JSON data.

    Returns
    -------
    list
        List of detected errors.
    """

    errors = []

    # fnId2Name maps function IDs to names
    fnid_to_name = data.get("fnId2Name", {})

    # Convert keys to integers for faster lookup
    valid_phase_ids = set(int(k) for k in fnid_to_name.keys())

    nodes = data.get("nodes", [])

    for node in nodes:
        node_id = node.get("id")

        inst_access = node.get("instAccess", {})

        for inst_id, event in inst_access.items():

            phase_fnid = event.get("PhaseFnId")

            # PhaseFnId must exist
            if phase_fnid is None:
                errors.append(
                    f"Node {node_id}, inst {inst_id}: missing PhaseFnId"
                )
                continue

            if phase_fnid not in valid_phase_ids:
                errors.append(
                    f"Node {node_id}, inst {inst_id}: invalid PhaseFnId {phase_fnid}"
                )

    return errors


def main():
    """
    Main program entry.

    1. Loads the JSON file
    2. Validates PhaseFnId references
    3. Reports errors
    """

    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <ir.json>")
        sys.exit(1)

    json_path = sys.argv[1]

    data = load_json(json_path)

    errors = validate_phase_ids(data)

    if errors:
        print("Invalid PhaseFnId entries detected:\n")

        for err in errors:
            print(err)

        print(f"\nTotal errors: {len(errors)}")

        sys.exit(1)

    print("All PhaseFnId values are valid.")


if __name__ == "__main__":
    main()
