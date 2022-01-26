#ifndef _INFO_SEL_TRACE_H_
#define _INFO_SEL_TRACE_H_

#include "pin.H"
#include "TraceFileHeader.h"
#include "RegVector.h"
#include "Trace.h"
#include "StringTable.h"
#include "IRModelerAPI.h"

// Tracer initialization functions
bool checkInitializedStatus(THREADID tid);
void initThread(THREADID tid, const CONTEXT *ctx);
void PIN_FAST_ANALYSIS_CALL initIns(THREADID tid);

// Record functions
void recordRegState(THREADID tid, const CONTEXT *ctxt);
void PIN_FAST_ANALYSIS_CALL recordSrcRegs(
        THREADID tid, const CONTEXT *ctx, const RegVector *srcRegs, UINT32 fnId, UINT32 instOpcode);
void PIN_FAST_ANALYSIS_CALL recordSrcId(THREADID tid, UINT32 srcId);
void PIN_FAST_ANALYSIS_CALL recordDestRegs(THREADID tid, const RegVector *destRegs, UINT32 destFlags);
void recordMemWrite(THREADID tid, ADDRINT addr, UINT32 size);
void checkMemRead(ADDRINT readAddr, UINT32 readSize, UINT32 fnId);
void check2MemRead(ADDRINT readAddr1, ADDRINT readAddr2, UINT32 readSize, UINT32 fnId);

// Analysis functions
bool analyzeRecords(THREADID tid, const CONTEXT *ctx, UINT32 fnId, UINT32 opcode, bool is_range);
void analyzeRegWrites(THREADID tid, const CONTEXT *ctx);
void analyzeMemWrites(THREADID tid, UINT32 fnId, bool is_range);
void analyzeRegReads();

//handles architectural events
void contextChange(
        THREADID tid, CONTEXT_CHANGE_REASON reason, const CONTEXT *fromCtx, 
        CONTEXT *toCtx, INT32 info, void *v);
void startTrace();
VOID threadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v);

//handling the trace format
void setupFile(UINT16 infoSelect);
void endFile();

// ==========================================================

const UINT32  UINT32_INVALID  = -1;
const ADDRINT ADDRINT_INVALID = -1;
const int     INT_INVALID     = -1;

// IR Modeling
const UINT32 V8 = 1;
const UINT32 JSC = 2;

const UINT32 MEMSIZE    = 8;

const int MAX_NODES     = 1000;     // Max number of nodes.
const int MAX_LOCS      = 100;      // Max number of locations between block head & tail.

const std::string SYSTEM_V8 = "v8";
const std::string V8_JIT = "compiler";
const std::string SYSTEM_JSC = "JSC";
const std::string JSC_JIT = "DFG";

const int V8_OPCODE_SIZE = 2;
const int JSC_OPCODE_SIZE = 4;

const std::string NODE_FORMERS[3] = {
    "v8::internal::compiler::Node::New",
    "v8::internal::Zone::NewExpand",
    "JSC::DFG::Node::Node",
};

const std::string NODE_BLOCK_ALLOCATORS[2] = {
    "v8::internal::compiler::Node::New",
    "bmalloc::BumpAllocator::allocate",
};

const std::string MAIN_NODE_CREATORS[2] { 
    "v8::internal::compiler::Node::New",
    "JSC::DFG::Node::Node"
};

const int NODE_FORMERS_SIZE = 3;
const int NODE_ALLOC_SIZE = 2;
const int NODE_CREATORS_SIZE = 2;

struct Node {
    Node() : 
        id(-1), alive(true), numberOfEdges(0), numberOfLocs(0), numberOfAdds(0), numberOfReplaces(0), numberOfRemoves(0) {}

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

// Main modeled IR constructor function.
void constructModeledIRNode(UINT32 fnId, UINT32 system_id);
void trackOptimization(ADDRINT location, ADDRINT value);


// API functions for system-specifics.
ADDRINT get_node_address(UINT32 fnId, UINT32 system_id);
ADDRINT get_opcode(Node *node, UINT32 system_id);
ADDRINT get_size(ADDRINT address, UINT32 system_id);
ADDRINT get_node_block_head(ADDRINT address, UINT32 system_id);
void    get_init_block_locs(Node *node, UINT32 system_id);

// Above functions call functions below to handle system-specific information extraction.
// To handle other system's opcode extraction, add function below and call from get_opcode(..).

// Functions for V8 goes here.
ADDRINT get_address_v8();
ADDRINT get_opcode_v8(Node *node);
ADDRINT get_size_v8(ADDRINT address);
ADDRINT get_node_block_head_v8(ADDRINT address);
// Functions for JSC goes here.
ADDRINT get_address_jsc();
ADDRINT get_opcode_jsc(Node *node);
ADDRINT get_size_jsc(ADDRINT address);

// Helper functions.
ADDRINT uint8Toaddrint(UINT8* target, UINT32 size);
UINT8* addrintTouint8(ADDRINT target, UINT32 size);
bool elemInMap(ADDRINT elem, std::map<ADDRINT,ADDRINT> targetMap);
bool fnInAllocs(std::string fn);
bool fnInFormers(std::string fn);
int compareValuetoIRNodes(ADDRINT value);
int get_edge_idx(Node *node, ADDRINT address);
bool is_direct_assignment(Node *node, ADDRINT value);

// Prints for debugging.
void printUINT8(UINT8 *currentRaxVal, UINT32 currentRaxValSize);
void printNode(Node *node);
void printMap(std::map<ADDRINT,ADDRINT> mymap);

extern StringTable strTable;

#endif
