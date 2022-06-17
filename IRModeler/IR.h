#ifndef _IR_
#define _IR_

#include "pin.H"
#include <map>

const int MAX_NODES     = 10000;     // Max number of nodes.
const int MAX_NODE_SIZE = 500;      // Max number of locations between block head & tail.

enum Access {
    INVALID=-1,
    ADDITION=0,
    REMOVAL=1,
    REPLACE=2,
    KILL=3,
    VALUE_CHANGE=4,
    EVALUATE=5,
    OP_UPDATE=6
};

struct FnInfo {
    FnInfo(): fnId(-1), accessType(INVALID) {}

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

struct ReplacedInfo {
    ReplacedInfo(): nodeIdFrom(-1), nodeIdTo(-1) {}

    int nodeIdFrom;     // node id that exist in the edge to be repleced.
    int nodeIdTo;       // node id that is replacing the existing edge node.
};

struct Node {
    Node() : 
        id(-1), alive(true), opcode(-1), opcodeAddress(-1), opcodeId(0), is_nonIR(false), numberOfEdges(0), numberOfLocs(0), lastInfoId(0) {}

    // Basic information.
    int     id;                            // node id = index of IRGraph->nodes.
    bool    alive;                         // tracks either the node is alive or dead.
    ADDRINT intAddress;                    // address in ADDRINT type.
    ADDRINT opcode;                        // node's opcode.
    ADDRINT opcodeAddress;                 // tracks opcode address.
    int     opcodeId;                      // tracks opcode Id. The initial opcode ID = 0.
    bool    is_nonIR;                      // tracks whether the current node is a IR node or not.
    // Structure information.
    UINT32  size;                          // size of a node.
    Node    *edgeNodes[MAX_NODES];         // list of nodes connected to this node.
    ADDRINT edgeAddrs[MAX_NODES];          // list of edge addresses.
    int     numberOfEdges;                 // number of edges.
    ADDRINT blockHead;                     // address of the node block head.
    ADDRINT blockTail;                     // address of the node block tail.
    // Metadata information.
    ADDRINT offsets[MAX_NODE_SIZE];        // tracks occupied memory locations between the block head & tail.
    ADDRINT valuesInLocs[MAX_NODE_SIZE];   // tracks values written to memory locations.
    int numberOfLocs;                      // number of occupied locations.
    // Optimization Information.
    std::map<int, int> fnOrder2addNodeId;           // track the function order id to the id of added node.
    std::map<int, int> fnOrder2remNodeId;           // track the function order id to the id of removed node.
    std::map<int, ReplacedInfo> fnOrder2repInfo;    // track the function order id to the replaced info object.
    std::map<int, DirectValOpt> fnOrder2dirValOpt;  // track the direct value change due to optimization.
    std::map<int, ADDRINT> id2Opcode;               // track the opcode update information during optimization.
    // Logging information.
    std::map<int, FnInfo> fnInfo;          // track of the functions accessed to this node.
    int lastInfoId;                        // track the ID assigned to the fnInfo added latest.
};

struct IR {
    IR() : id(-1), lastNodeId(0), fnOrderId(0), systemId(-1) {}

    int         id;                             // ir graph id.
    Node        *nodes[MAX_NODES];              // array holding ptrs to node objects.
    ADDRINT     nodeAddrs[MAX_NODES];           // array holding node addresses. For easy look up.
    int         lastNodeId;                     // id of a last node in the array.
    int         fnOrderId;                      // id indicating the order of function access.
    std::map<UINT32, std::string> fnId2Name;    // map to hold function id to name for look up.
    UINT32      systemId;                       // JIT compiler system ID.
};

#endif
