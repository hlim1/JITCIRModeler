# Default values

DEFAULT_ID = -1
DEFAULT_NUM = 0
DEFAULT_ALIVE = True
DEFAULT_EMPTY = None

class IRGraph:
    def __init__(self):
        self.id    = DEFAULT_ID
        self.nodes = []

class Node:
    def __init__(self):
        self.graph_id = DEFAULT_ID
        self.id = DEFAULT_ID
        self.alive = DEFAULT_ALIVE
        self.size = DEFAULT_NUM
        self.opcode = DEFAULT_EMPY
        self.address = DEFAULT_EMPTY
        self.edges = []
        self.added_nodes = []
        self.removed_nodes = []
        self.replaced_nodes = {}
        self.occupied = {}
