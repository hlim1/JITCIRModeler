#ifndef _IR_
#define _IR_

#include "pin.H"
#include <map>

const int MAX_NODES     = 2000;     // Max number of nodes.
const int MAX_NODE_SIZE = 500;      // Max number of locations between block head & tail.

enum Access {
    INVALID=-1,
    ADDITION=0,
    REMOVAL=1,
    REPLACE=2,
    KILL=3,
    VALUE_CHANGE=4,
    EVALUATE=5,
    OP_UPDATE=6,
    CREATE=7
};

struct InstInfo {
    InstInfo(): fnCallRetId(0), accessType(INVALID) {}

    int fnCallRetId;    // Function call-return id, i.e., index in the sequence.
    UINT32 fnId;        // Function id.
    UINT8* binary;      // Instruction in binary (opcode & operands).
    ADDRINT instSize;   // Instruction size.
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

struct Offset2Value {
    Offset2Value(): offset(-1), value(-1) {}

    ADDRINT offset;     // offset location of node.
    ADDRINT value;      // value in the offset location.
};

struct Node {
    Node() : 
        id(-1), alive(true), opcode(-1), opcodeAddress(-1), is_nonIR(false), numberOfEdges(0), numberOfLocs(0), lastInfoId(0) {}

    // Basic information.
    int     id;                            // node id = index of IRGraph->nodes.
    bool    alive;                         // tracks either the node is alive or dead.
    ADDRINT intAddress;                    // address in ADDRINT type.
    ADDRINT opcode;                        // node's opcode.
    ADDRINT opcodeAddress;                 // tracks opcode address.
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
    std::map<int, int> instOrder2addNodeId;          // track the inst. order id to the id of added node.
    std::map<int, int> instOrder2remNodeId;          // track the inst. order id to the id of removed node.
    std::map<int, ReplacedInfo> instOrder2repInfo;   // track the inst. order id to the replaced info object.
    std::map<int, DirectValOpt> instOrder2dirValOpt; // track the direct value change due to optimization.
    std::map<int, ADDRINT> id2Opcode;                // track the opcode update information during optimization.
    std::map<int, Offset2Value> instOrder2offVal;    // track the offset accessed by the memory read action.
    // Logging information.
    std::map<int, InstInfo> instInfo;      // track of the instructions accessed (mem. read/write) to this node.
    int lastInfoId;                        // track the ID assigned to the fnInfo added latest.
};

struct IR {
    IR() : id(-1), lastNodeId(0), systemId(-1) {}

    int         id;                             // ir graph id.
    Node        *nodes[MAX_NODES];              // array holding ptrs to node objects.
    ADDRINT     nodeAddrs[MAX_NODES];           // array holding node addresses. For easy look up.
    int         lastNodeId;                     // id of a last node in the array.
    UINT32      systemId;                       // JIT compiler system ID.
};

#endif
