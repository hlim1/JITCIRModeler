#ifndef _IR_
#define _IR_

#include "pin.H"
#include <map>

const int MAX_NODES     = 1000;     // Max number of nodes.
const int MAX_LOCS      = 100;      // Max number of locations between block head & tail.

enum Access {
    ADDITION,
    REMOVAL,
    REPLACE,
    KILL,
    VALUE_CHANGE,
    EVALUATE
};

struct FnInfo {
    UINT32 fnId;        // function id that can be looked up in the `fnId2Name` map of IR.
    Access accessType;  // function access type.
};

struct DirectValOpt {
    DirectValOpt(): offset(-1), valFrom(-1), valTo(-1), is_update(false) {}

    ADDRINT offset;     // offset of the value written to.
    ADDRINT valFrom;    // value changed from. if there is from value, then default set to -1.
    ADDRINT valTo;      // value changed to.
    bool is_update;     // set to true if is updating the exsiting offset. Otherwise, false.
};

struct Node {
    Node() : 
        id(-1), alive(true), numberOfEdges(0), numberOfLocs(0),
        numberOfReplaces(0), numberOfRemoves(0) {}

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
    ADDRINT offsets[MAX_LOCS];             // tracks occupied memory locations between the block head & tail.
    ADDRINT valuesInLocs[MAX_LOCS];        // tracks values written to memory locations.
    int numberOfLocs;                      // number of occupied locations.
    // Optimization Information.
    int     numberOfReplaces;                       // counter to track the number of edge replace happend.
    int     numberOfRemoves;                        // counter to track the number of edge remove happend.
    int     removedNodeIds[MAX_NODES];              // track the node ids removed from this node edge(s).
    int     removedEdgeIdx[MAX_NODES];              // track the index of removed node edge in the edgeNodes.
    int     replacedNodeIds[MAX_NODES][2];          // track the node ids replaced from and to this node edge(s).
    int     replacedEdgeIdx[MAX_NODES];             // track the index of replaced node edde in the edgeNodes.

    std::map<int, int> fnOrder2addNodeId;           // track the function order id to the id of added node.
    std::map<int, DirectValOpt> fnOrder2dirValOpt;  // track the direct value change due to optimization.
    // Logging information.
    std::map<int, FnInfo> fnInfo;          // track of the functions accessed to this node.
};

struct IR {
    IR() : id(-1), lastNodeId(0), fnOrderId(0) {}

    int         id;                             // ir graph id.
    Node        *nodes[MAX_NODES];              // array holding ptrs to node objects.
    ADDRINT     nodeAddrs[MAX_NODES];           // array holding node addresses. For easy look up.
    int         lastNodeId;                     // id of a last node in the array.
    int         fnOrderId;                      // id indicating the order of function access.
    std::map<UINT32, std::string> fnId2Name;    // map to hold function id to name for look up.
};

#endif
