#ifndef _IRMODELERAPI_
#define _IRMODELERAPI_

#include "pin.H"
#include "TraceFileHeader.h"
#include "RegVector.h"
#include "Trace.h"
#include "StringTable.h"
#include "IR.h"

const UINT32  UINT32_INVALID  = -1;
const ADDRINT ADDRINT_INVALID = -1;
const int     INT_INVALID     = -1;

const UINT32 V8 = 1;
const UINT32 JSC = 2;

const UINT32 MEMSIZE    = 8;

const std::string SYSTEM_V8 = "v8";
const std::string V8_JIT = "compiler";
const std::string SYSTEM_JSC = "JSC";
const std::string JSC_JIT = "DFG";

const int V8_OPCODE_SIZE = 2;
const int JSC_OPCODE_SIZE = 4;

const std::string NODE_FORMERS[2] = {
    "v8::internal::compiler::Node::New",
    "JSC::DFG::Node::Node"
};

const std::string NODE_BLOCK_ALLOCATORS[2] = {
    "v8::internal::compiler::Node::New",
    "bmalloc::BumpAllocator::allocate"
};

const std::string MAIN_NODE_CREATORS[2] { 
    "v8::internal::compiler::Node::New",
    "JSC::DFG::Node::Node"
};

const int NODE_FORMERS_SIZE = 2;
const int NODE_ALLOC_SIZE = 2;
const int NODE_CREATORS_SIZE = 2;

// Main modeled IR constructor function.
void constructModeledIRNode(UINT32 fnId, UINT32 system_id);


// API functions for system-specifics.
ADDRINT get_node_address(UINT32 fnId, UINT32 system_id);
ADDRINT *get_opcode(Node *node, UINT32 system_id);
ADDRINT get_size(ADDRINT address, UINT32 system_id);
ADDRINT get_node_block_head(ADDRINT address, UINT32 system_id);
void    get_init_block_locs(Node *node, UINT32 system_id);

// Above functions call functions below to handle system-specific information extraction.
// To handle other system's opcode extraction, add function below and call from get_opcode(..).

// Functions for V8 goes here.
ADDRINT get_address_v8();
ADDRINT *get_opcode_v8(Node *node);
ADDRINT get_size_v8(ADDRINT address);
ADDRINT get_node_block_head_v8(ADDRINT address);
// Functions for JSC goes here.
ADDRINT get_address_jsc();
ADDRINT *get_opcode_jsc(Node *node);
ADDRINT get_size_jsc(ADDRINT address);

// Optimization functions.
void trackOptimization(ADDRINT location, ADDRINT value, ADDRINT valueSize, UINT32 fnId, UINT32 system_id);
void edgeRemoval(Node *node, int edge_idx, UINT32 fnId);
void edgeReplace(Node *node, int value_id, int edge_idx, UINT32 fnId);
void edgeAddition(Node *node, ADDRINT location, int value_id, UINT32 fnId);
void nodeDestroy(Node *node, UINT32 fnId);
void directValueWrite(Node *node, ADDRINT location, ADDRINT value, UINT32 fnId);
void opcodeUpdate(Node *node, ADDRINT value, ADDRINT valueSize, UINT32 system_id);

// Helper functions.
ADDRINT uint8Toaddrint(UINT8* target, UINT32 size);
UINT8*  addrintTouint8(ADDRINT target, UINT32 size);
bool    elemInMap(ADDRINT elem, std::map<ADDRINT,ADDRINT> targetMap);
bool    fnInAllocs(std::string fn);
bool    fnInFormers(std::string fn);
int     compareValuetoIRNodes(ADDRINT value);
int     getEdgeEdx(Node *node, ADDRINT address);
bool    isDirectAssignment(ADDRINT value);
void    updateLogInfo(Node *node, UINT32 fnId, Access accessType);
bool    isSameAccess(Node *node, FnInfo fnInfo);
ADDRINT getUpdatedOpcode(ADDRINT value, ADDRINT valueSize, UINT32 system_id);

// Prints for debugging.
void printUINT8(UINT8 *currentRaxVal, UINT32 currentRaxValSize);
void printNodes();
void printNode(Node *node);
void printMap(std::map<ADDRINT,ADDRINT> mymap);

extern StringTable strTable;

#endif
