#!/usr/bin/env python3
"""
draw_phase_graphs.py

Author: Terrence Lim (with the help of LLM).
Created: 2026-03-09

Description
-----------
This script parses an `ir.json` file, groups nodes by the compiler phase in
which they were created, reconstructs the initial edge connections of each
node using `initialEdges`, and draws one graph per phase.

Each phase graph contains:
- nodes created in that phase
- directed edges based on `initialEdges`

A separate PNG file is generated for each phase.

The node creation phase is identified from the node's `instAccess` log.
A creation event is an instruction access entry with:

    type = 7   (CREATE)

The phase name is resolved using `fnId2Name`.

Output
------
The script creates an output directory and writes one PNG file per phase:

    <output_dir>/
        000_<phase_name>.png
        001_<phase_name>.png
        002_<phase_name>.png
        ...

Requirements
------------
- Python 3.x
- graphviz Python package
- Graphviz installed on the system

Install Python package:
    pip install graphviz

Install Graphviz system package:
    Ubuntu/Debian: sudo apt install graphviz
    macOS (Homebrew): brew install graphviz

Usage
-----
    python3 draw_phase_graphs.py ir.json output_dir

Example
-------
    python3 draw_phase_graphs.py ir.json phase_graphs

Notes
-----
- Only nodes created in a given phase are drawn in that phase's graph.
- Only `initialEdges` are used for graph construction.
- If an edge points to a node outside the current phase group, that target
  node is still drawn so the connection remains visible.

"""

import json
import os
import re
import sys
from collections import defaultdict
from graphviz import Digraph

CREATE = 7
INT_INVALID = -1


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
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def sanitize_filename(name):
    """
    Convert a phase name into a filesystem-safe filename fragment.

    Parameters
    ----------
    name : str
        Original phase name.

    Returns
    -------
    str
        Sanitized filename-safe string.
    """
    sanitized = re.sub(r"[^A-Za-z0-9._-]+", "_", name).strip("_")
    return sanitized if sanitized else "unknown_phase"


def get_creation_phase_fnid(node):
    """
    Return the phaseFnId of the CREATE event for a node.

    Parameters
    ----------
    node : dict
        JSON object representing one node.

    Returns
    -------
    int
        phaseFnId of the node creation event.

    Raises
    ------
    AssertionError
        If the node has no instAccess entries, no CREATE event,
        or the CREATE event has no phaseFnId.
    """
    inst_access = node.get("instAccess", {})
    node_id = node.get("id")

    assert inst_access, f"Node {node_id} has empty instAccess."

    sorted_items = sorted(inst_access.items(), key=lambda x: int(x[0]))

    create_event = None
    for _, event in sorted_items:
        if event.get("type") == CREATE:
            create_event = event
            break

    assert create_event is not None, f"Node {node_id} has no CREATE (type 7) event."

    phase_fnid = create_event.get("phaseFnId")
    assert phase_fnid is not None, f"Node {node_id} CREATE event missing PhaseFnId."

    return phase_fnid


def build_node_lookup(data):
    """
    Build a mapping from node id to node object.

    Parameters
    ----------
    data : dict
        Parsed JSON object.

    Returns
    -------
    dict
        Mapping: node_id -> node dictionary
    """
    lookup = {}
    for node in data.get("nodes", []):
        node_id = node.get("id")
        if node_id is not None:
            lookup[node_id] = node
    return lookup


def group_nodes_by_creation_phase(data):
    """
    Group nodes by their creation phase.

    Parameters
    ----------
    data : dict
        Parsed JSON object.

    Returns
    -------
    tuple
        (phase_to_nodes, fnid_to_name)

        phase_to_nodes : dict
            Mapping from PhaseFnId to list of node dictionaries.

        fnid_to_name : dict
            Mapping from function id string to function name.
    """
    fnid_to_name = data.get("fnId2Name", {})
    phase_to_nodes = defaultdict(list)

    for node in data.get("nodes", []):
        phase_fnid = get_creation_phase_fnid(node)
        phase_to_nodes[phase_fnid].append(node)

    return phase_to_nodes, fnid_to_name


def add_node_to_graph(dot, node_id, node_lookup, phase_node_ids):
    """
    Add a node to the Graphviz graph.

    Parameters
    ----------
    dot : graphviz.Digraph
        Graphviz graph object.
    node_id : int
        Node id to add.
    node_lookup : dict
        Mapping from node id to node object.
    phase_node_ids : set
        Set of node ids created in the current phase.
    """
    node = node_lookup.get(node_id)

    if node is None:
        label = f"{node_id}\\n<missing>"
        shape = "ellipse"
    else:
        opcode = node.get("opcode", "")
        name = node.get("mnemonic", "")
        label = f"{node_id}"
        if opcode != "":
            label += f"\\nop=0x{opcode}"
        if name != "":
            label += f"\\n({name})"
        shape = "box" if node_id in phase_node_ids else "ellipse"

    dot.node(str(node_id), label=label, shape=shape)


def draw_phase_graph(phase_fnid, phase_name, phase_nodes, node_lookup, output_dir, index):
    """
    Draw and save one graph for a phase using initialEdges.

    Parameters
    ----------
    phase_fnid : int
        Phase function id.
    phase_name : str
        Human-readable phase name.
    phase_nodes : list
        List of node dictionaries created in this phase.
    node_lookup : dict
        Mapping from node id to node dictionary.
    output_dir : str
        Directory where output PNG file is written.
    index : int
        Sequential index used in output filename.

    Returns
    -------
    str
        Path to the generated PNG file.
    """
    safe_phase_name = sanitize_filename(phase_name)
    base_filename = f"{index:03d}_{safe_phase_name}"
    output_path_no_ext = os.path.join(output_dir, base_filename)

    dot = Digraph(name=base_filename, format="png")
    dot.attr(rankdir="LR")
    dot.attr(label=f"{phase_name}\\n(PhaseFnId={phase_fnid})", labelloc="t", fontsize="20")

    phase_node_ids = {node["id"] for node in phase_nodes if "id" in node}
    drawn_nodes = set()

    # First add all nodes created in this phase.
    for node_id in sorted(phase_node_ids):
        add_node_to_graph(dot, node_id, node_lookup, phase_node_ids)
        drawn_nodes.add(node_id)

    # Then add edges based on initialEdges.
    for node in sorted(phase_nodes, key=lambda n: n.get("id", -1)):
        dst_id = node.get("id")
        initial_edges = node.get("initialEdges", [])

        if initial_edges is None:
            initial_edges = []

        for position, target_id in enumerate(initial_edges):
            if target_id == INT_INVALID or target_id is None:
                continue

            if target_id not in drawn_nodes:
                add_node_to_graph(dot, target_id, node_lookup, phase_node_ids)
                drawn_nodes.add(target_id)

            # Edge direction: created node -> outgoing target node
            dot.edge(str(dst_id), str(target_id), label=str(position))

    dot.render(output_path_no_ext, cleanup=True)
    return output_path_no_ext + ".png"


def main():
    """
    Main entry point.

    This function:
    1. Loads the IR JSON file
    2. Groups nodes by their creation phase
    3. Reconstructs initial edges using `initialEdges`
    4. Draws one graph per phase
    5. Saves each graph as a PNG file
    """
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <ir.json> <output_dir>")
        sys.exit(1)

    json_path = sys.argv[1]
    output_dir = sys.argv[2]

    os.makedirs(output_dir, exist_ok=True)

    data = load_json(json_path)
    node_lookup = build_node_lookup(data)
    phase_to_nodes, fnid_to_name = group_nodes_by_creation_phase(data)

    sorted_phase_ids = sorted(phase_to_nodes.keys(), key=int)

    for index, phase_fnid in enumerate(sorted_phase_ids):
        phase_name = fnid_to_name.get(str(phase_fnid), f"unknown_phase")
        
        if phase_name != "unknown_phase":
            phase_name = phase_name.split("::")[3]

        phase_nodes = phase_to_nodes[phase_fnid]

        output_png = draw_phase_graph(
            phase_fnid=phase_fnid,
            phase_name=phase_name,
            phase_nodes=phase_nodes,
            node_lookup=node_lookup,
            output_dir=output_dir,
            index=index,
        )

        print(f"Wrote {output_png}")


if __name__ == "__main__":
    main()
