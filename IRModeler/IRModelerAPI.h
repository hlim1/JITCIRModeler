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
const UINT32 SPM = 3;

const UINT32 MEMSIZE    = 8;

const std::string V8_JIT = "v8::internal::compiler";
const std::string JSC_JIT = "JSC::DFG";
const std::string SPM_JIT = "js::jit";

const int V8_OPCODE_SIZE = 2;
const int JSC_OPCODE_SIZE = 4;
const int SPM_OPCODE_SIZE = 2;

const std::string NODE_FORMERS[1] = {
    "JSC::DFG::Node::Node"
};

const std::string NODE_BLOCK_ALLOCATORS[46] = {
    "v8::internal::compiler::Node::New",
    "bmalloc::BumpAllocator::allocate",
    "js::jit::MBasicBlock::New",
    "js::jit::MGoto::New",
    "js::jit::MParameter::New<int const&>",
    "js::jit::MConstant::New",
    "js::jit::MConstant::MConstant",
    "js::jit::MToDouble::New<js::jit::MDefinition*&>",
    "js::jit::MAdd::New<js::jit::MDefinition*&, js::jit::MConstant*&, js::jit::MIRType>",
    "js::jit::MSub::New<js::jit::MDefinition*&, js::jit::MDefinition*&, js::jit::MIRType>",
    "js::jit::MDiv::New",
    "js::jit::MMul::New",
    "js::jit::MMod::New",
    "js::jit::MOsrEntry::New<>",
    "js::jit::MReturn::New<js::jit::MDefinition*&>",
    "js::jit::MEnclosingEnvironment::New",
    "js::jit::MGuardShape::New<js::jit::MDefinition*&, js::Shape*&>",
    "js::jit::MSlots::New<js::jit::MDefinition*&>",
    "js::jit::MLoadDynamicSlot::New<js::jit::MSlots*&, unsigned long&>",
    "js::jit::MCompare::New<js::jit::MDefinition*&, js::jit::MDefinition*&, JSOp&, js::jit::MCompare::CompareType&>",
    "js::jit::MCall::New",
    "js::jit::MPostWriteBarrier::New<js::jit::MDefinition*&, js::jit::MDefinition*&>",
    "js::jit::MBox::New<js::jit::MDefinition*&>",
    "js::jit::MLimitedTruncate::New<js::jit::MDefinition*&, js::jit::TruncateKind>",
    "js::jit::MUnbox::New",
    "js::jit::MUnreachableResult::New<js::jit::MIRType&>",
    "js::jit::MCheckReturn::New<js::jit::MDefinition*&, js::jit::MDefinition*&>",
    "js::jit::MCheckThis::New<js::jit::MDefinition*&>",
    "js::jit::MCheckThisReinit::New<js::jit::MDefinition*&>",
    "js::jit::MSuperFunction::New<js::jit::MDefinition*&>",
    "js::jit::MBigIntPow::New<js::jit::MDefinition*&, js::jit::MDefinition*&>",
    "js::jit::MAbs::New<js::jit::MDefinition*&, js::jit::MIRType>",
    "js::jit::MMathFunction::New<js::jit::MDefinition*&, js::UnaryMathFunction&>",
    "js::jit::MSqrt::New<js::jit::MDefinition*&, js::jit::MIRType>",
    "js::jit::MCeil::New<js::jit::MDefinition*&>",
    "js::jit::MFloor::New<js::jit::MDefinition*&>",
    "js::jit::MRound::New<js::jit::MDefinition*&>",
    "js::jit::MNewArray::MNewArray",
    "js::jit::MToNumberInt32::New<js::jit::MDefinition*&, js::jit::IntConversionInputKind>",
    "js::jit::MStoreElement::New<js::jit::MElements*&, js::jit::MConstant*&, js::jit::MDefinition*&, bool>",
    "js::jit::MToFloat32::New<js::jit::MDefinition*&>",
    "js::jit::MPhi::New",
    "js::jit::MSlots::New<js::jit::MDefinition*&>",
    "js::jit::MNearbyInt::New<js::jit::MDefinition*&, js::jit::MIRType, js::jit::RoundingMode>",
    "js::jit::MNot::New<js::jit::MDefinition*&>",
    "js::jit::MTest::New<js::jit::MDefinition*, js::jit::MBasicBlock*, js::jit::MBasicBlock*>"
};

const std::string MAIN_NODE_CREATORS[46] { 
    "v8::internal::compiler::Node::New",
    "JSC::DFG::Node::Node",
    "js::jit::MBasicBlock::New",
    "js::jit::MGoto::New",
    "js::jit::MParameter::New<int const&>",
    "js::jit::MConstant::New",
    "js::jit::MConstant::MConstant",
    "js::jit::MToDouble::New<js::jit::MDefinition*&>",
    "js::jit::MAdd::New<js::jit::MDefinition*&, js::jit::MConstant*&, js::jit::MIRType>",
    "js::jit::MSub::New<js::jit::MDefinition*&, js::jit::MDefinition*&, js::jit::MIRType>",
    "js::jit::MDiv::New",
    "js::jit::MMul::New",
    "js::jit::MMod::New",
    "js::jit::MOsrEntry::New<>",
    "js::jit::MReturn::New<js::jit::MDefinition*&>",
    "js::jit::MEnclosingEnvironment::New",
    "js::jit::MGuardShape::New<js::jit::MDefinition*&, js::Shape*&>",
    "js::jit::MSlots::New<js::jit::MDefinition*&>",
    "js::jit::MLoadDynamicSlot::New<js::jit::MSlots*&, unsigned long&>",
    "js::jit::MCompare::New<js::jit::MDefinition*&, js::jit::MDefinition*&, JSOp&, js::jit::MCompare::CompareType&>",
    "js::jit::MCall::New",
    "js::jit::MPostWriteBarrier::New<js::jit::MDefinition*&, js::jit::MDefinition*&>",
    "js::jit::MBox::New<js::jit::MDefinition*&>",
    "js::jit::MLimitedTruncate::New<js::jit::MDefinition*&, js::jit::TruncateKind>",
    "js::jit::MUnbox::New",
    "js::jit::MUnreachableResult::New<js::jit::MIRType&>",
    "js::jit::MCheckReturn::New<js::jit::MDefinition*&, js::jit::MDefinition*&>",
    "js::jit::MCheckThis::New<js::jit::MDefinition*&>",
    "js::jit::MCheckThisReinit::New<js::jit::MDefinition*&>",
    "js::jit::MSuperFunction::New<js::jit::MDefinition*&>",
    "js::jit::MBigIntPow::New<js::jit::MDefinition*&, js::jit::MDefinition*&>",
    "js::jit::MAbs::New<js::jit::MDefinition*&, js::jit::MIRType>",
    "js::jit::MMathFunction::New<js::jit::MDefinition*&, js::UnaryMathFunction&>",
    "js::jit::MSqrt::New<js::jit::MDefinition*&, js::jit::MIRType>",
    "js::jit::MCeil::New<js::jit::MDefinition*&>",
    "js::jit::MFloor::New<js::jit::MDefinition*&>",
    "js::jit::MRound::New<js::jit::MDefinition*&>",
    "js::jit::MNewArray::MNewArray",
    "js::jit::MToNumberInt32::New<js::jit::MDefinition*&, js::jit::IntConversionInputKind>",
    "js::jit::MStoreElement::New<js::jit::MElements*&, js::jit::MConstant*&, js::jit::MDefinition*&, bool>",
    "js::jit::MToFloat32::New<js::jit::MDefinition*&>",
    "js::jit::MPhi::New",
    "js::jit::MSlots::New<js::jit::MDefinition*&>",
    "js::jit::MNearbyInt::New<js::jit::MDefinition*&, js::jit::MIRType, js::jit::RoundingMode>",
    "js::jit::MNot::New<js::jit::MDefinition*&>",
    "js::jit::MTest::New<js::jit::MDefinition*, js::jit::MBasicBlock*, js::jit::MBasicBlock*>"
};

const std::string NONIR_NODE_ALLOCATORS[2] = {
    "js::jit::MBasicBlock::New",
    "js::jit::MResumePoint::New"
};

const int NODE_FORMERS_SIZE = 1;
const int NODE_ALLOC_SIZE = 46;
const int NODE_CREATORS_SIZE = 46;
const int NONIR_NODE_ALLOC_SIZE = 2;

// Main modeled IR constructor function.
void constructModeledIRNode(UINT32 fnId, UINT8* binary, ADDRINT instSize, UINT32 system_id, ADDRINT addr);


// API functions for system-specifics.
ADDRINT get_node_address(UINT32 fnId, UINT32 system_id);
ADDRINT *get_opcode(Node *node, UINT32 system_id, UINT32 fnId);
ADDRINT get_size(ADDRINT address, UINT32 system_id);
ADDRINT get_node_block_head(ADDRINT address, UINT32 system_id);
void    get_init_block_locs(Node *node, UINT32 system_id);
ADDRINT *get_updated_opcode(Node *node, ADDRINT location, ADDRINT value, ADDRINT valueSize, UINT32 system_id);
void    check_and_fix_opcode();

// Above functions call functions below to handle system-specific information extraction.
// To handle other system's opcode extraction, add function below and call from get_opcode(..).

// Functions for V8 goes here.
ADDRINT *get_opcode_v8(Node *node);
ADDRINT get_size_v8(ADDRINT address);
ADDRINT get_node_block_head_v8(ADDRINT address);
ADDRINT *get_update_opcode_v8(Node* node, ADDRINT value);
// Functions for JSC goes here.
ADDRINT *get_opcode_jsc(Node *node);
ADDRINT get_size_jsc(ADDRINT address);
ADDRINT *get_update_opcode_jsc(Node* node, ADDRINT location, ADDRINT value, ADDRINT valueSize);
// Functions for SpiderMonkey(SPM) goes here.
ADDRINT *get_opcode_spm(Node *node, UINT32 fnId);
ADDRINT get_size_spm(ADDRINT address);
ADDRINT *get_update_opcode_spm(Node* node, ADDRINT location, ADDRINT value, ADDRINT valueSize);

// Optimization functions.
void trackOptimization(
        ADDRINT location, ADDRINT value, ADDRINT valueSize, UINT32 fnId, UINT8* binary,
        ADDRINT instSize, UINT32 system_id, ADDRINT addr);
void edgeRemoval(Node *node, int edge_idx, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr);
void edgeReplace(Node *node, int value_id, int edge_idx, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr);
void edgeAddition(Node *node, ADDRINT location, int value_id, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr);
void nodeDestroy(Node *node, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr);
void directValueWrite(Node *node, ADDRINT location, ADDRINT value, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr);
void opcodeUpdate(
        Node *node, ADDRINT location, ADDRINT value, ADDRINT valueSize, UINT32 system_id,
        UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr);
void nodeEvaluation(ADDRINT readAddr, ADDRINT value, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr);

// Helper functions.
ADDRINT uint8Toaddrint(UINT8* target, UINT32 size);
UINT8*  addrintTouint8(ADDRINT target, UINT32 size);
bool    fnInAllocs(std::string fn);
bool    fnInFormers(std::string fn);
bool    fnInCreators(std::string fn);
bool    fnInNonIRAllocs(std::string fn);
int     compareValuetoIRNodes(ADDRINT value);
int     getEdgeEdx(Node *node, ADDRINT address);
bool    isMemoryWriteLoc(ADDRINT value);
void    updateLogInfo(Node *node, ADDRINT addr, UINT32 fnId, UINT8* binary, ADDRINT instSize, Access accessType);
bool    isSameAccess(Node *node, InstInfo instInfo);

// Prints for debugging.
void printUINT8(UINT8 *currentRaxVal, UINT32 currentRaxValSize);

extern StringTable strTable;

#endif
