#ifndef _IR_
#define _IR_

#include "pin.H"

const int MAX_NODES     = 1000;     // Max number of nodes.
const int MAX_LOCS      = 100;      // Max number of locations between block head & tail.

struct Node {
    Node() : 
        id(-1), alive(true), numberOfEdges(0), numberOfLocs(0),
        numberOfAdds(0), numberOfReplaces(0), numberOfRemoves(0) {}

    // Basic information.
    int     id;                            // node id = index of IRGraph->nodes.
    bool    alive;                         // tracks either the node is alive or dead.
    ADDRINT intAddress;                    // address in ADDRINT type.
    ADDRINT opcode;                        // node's opcode.
    // Structure information.
    UINT32  size;                          // size of a node.
    Node    *edgeNodes[MAX_NODES];         // list of nodes connected to this node.
    ADDRINT edgeAddrs[MAX_NODES];          // list of edge addresses.
    int     numberOfEdges;                 // number of edges.
    ADDRINT blockHead;                     // address of the node block head.
    ADDRINT blockTail;                     // address of the node block tail.
    // Metadata information.
    ADDRINT occupiedLocs[MAX_LOCS];        // tracks occupied memory locations between the block head & tail.
    ADDRINT valuesInLocs[MAX_LOCS];        // tracks values written to memory locations.
    int numberOfLocs;                      // number of occupied locations.
    // Optimization Information.
    int     numberOfAdds;                  // counter to track the number of edge added happend.
    int     numberOfReplaces;              // counter to track the number of edge replace happend.
    int     numberOfRemoves;               // counter to track the number of edge remove happend.
    int     addedNodeIds[MAX_NODES];       // track the node ids appended to this node's edge(s).
    int     removedNodeIds[MAX_NODES];     // track the node ids removed from this node's edge(s).
    int     removedEdgeIdx[MAX_NODES];     // track the index of removed node edge in the edgeNodes.
    int     replacedNodeIds[MAX_NODES][2]; // track the node ids replaced from and to this node's edge(s).
    int     replacedEdgeIdx[MAX_NODES];    // track the index of replaced node edde in the edgeNodes.
};

struct IR {
    IR() : id(-1), lastNodeId(0) {}

    int         id;                        // ir graph id.
    Node        *nodes[MAX_NODES];         // array holding ptrs to node objects.
    ADDRINT     nodeAddrs[MAX_NODES];      // array holding node addresses. For easy look up.
    int         lastNodeId;                // id of a last node in the array.
};


#endif
