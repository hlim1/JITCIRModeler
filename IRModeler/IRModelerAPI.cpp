/**
 * This file is intended to hold everything needed for the analysis functions of a format.
 * In addition, it includes everything that is needed to setup/finish the trace file because
 * historically the Main file contained code for multiple trace formats.
 **/

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include "PinLynxReg.h"
#include "IRModelerAPI.h"
#include "DataOpsDefs.h"
#include "Helpers.h"
#include "Tracer.h"

#include <cstring>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>

using std::cout;
using std::cerr;
using std::endl;
using std::map;
using std::string;
using std::hex;
using std::dec;
using std::ostringstream;

// This will need to be adjusted if there are more than 16 threads, 
// but this is faster than Pin's TLS
const UINT32 maxThreads = 16;

StringTable strTable;
const RegVector emptyRegVector;
UINT32 sectionOff;
UINT32 traceHeaderOff;
UINT32 traceStart;
UINT32 predTID = 0;
UINT32 numSkipped = 0;
const UINT32 LARGEST_REG_SIZE = 1024;
const UINT32 BUF_SIZE = 16384;
const UINT32 MAX_MEM_OP_SIZE = 1024;
const UINT32 UINT8_SIZE = sizeof(UINT8);
const UINT32 UINT16_SIZE = sizeof(UINT16);
const UINT32 UINT32_SIZE = sizeof(UINT32);
const UINT32 UINT64_SIZE = sizeof(UINT64);
const UINT32 ADDRINT_SIZE = sizeof(ADDRINT);
const UINT32 UINT32_INVALID_FLAG = -1;

UINT8 traceBuf[BUF_SIZE];
UINT8 *traceBufPos = traceBuf;
UINT8 dataBuf[BUF_SIZE];
UINT8 *dataBufPos = dataBuf;

#if defined(TARGET_IA32)
#pragma message("x86")
#define fullLynxReg LynxReg2FullLynxIA32Reg
#define lynxRegSize LynxRegSize32
#elif defined(TARGET_IA32E)
#pragma message("x86-64")
#define fullLynxReg LynxReg2FullLynxIA32EReg
#define lynxRegSize LynxRegSize
#else
#error "Unsupported Architecture"
#endif

// Thread local storage structure (for those unfamiliar with C++ structs, they are essentially
// classes with different privacy rules so you can have functions)
struct ThreadData {
    ThreadData() :
            eventId(0), pos(NULL), memWriteAddr(0), memWriteSize(0), destRegs(NULL), destFlags(0), print(false),
            initialized(false), predSrcId(0), predFuncId(0), predAddr(0), dataPos(NULL), printBinary(NULL), 
            binary(NULL) {}
    UINT64 eventId;
    UINT8 buffer[40];
    UINT8 *pos;
    UINT8 *predRecord;
    ADDRINT memWriteAddr;
    UINT32 memWriteSize;
    const RegVector *destRegs;
    UINT32 destFlags;
    bool print;
    bool initialized;
    uint32_t predSrcId;
    uint32_t predFuncId;
    ADDRINT predAddr;
    UINT8 dataBuffer[4 * MAX_MEM_OP_SIZE];
    UINT8 *dataPos; 
    bool *printBinary;
    uint8_t *binary;
    uint8_t binSize;
};

//Now we don't have to rely on PIN for TLS
ThreadData tls[maxThreads];

// =============================================================

IR *IRGraph = new IR();

// Declare consts here.
const string TARGET_REG = "RAX";
const LynxReg TARGET_LREG = LYNX_RAX;
const UINT32 ADD = XED_ICLASS_ADD;
const int MAX_REGS = 10;
const ADDRINT WIPEMEM = 0;

// Strcut for keeping the instruction id.
struct Instruction {
    Instruction() : id(0) {}
    int id;                   // instruction id.
} instruction;

// Struct for holding the target instruction info.
struct TargetInst {
    UINT32 fnId;               // function id.
    UINT8* binary;             // instruction opcode & operand.
};

// Struct for source register information.
struct RegInfo {
    int instId;                // instruction id.
    UINT32 fnId;               // function id.
    UINT32 instOp;             // instruction opcode.
    LynxReg lReg;              // lynx register.
    ADDRINT value;             // value in the register.
};

// Struct for memory write information.
struct MWInst {
    UINT32  fnId;              // function id.
    ADDRINT location;          // address location.
    ADDRINT value;             // written value.
    ADDRINT valueSize;         // size of written value in bytes.
    RegInfo  srcRegs[MAX_REGS]; // values in the read registers.
    int regSize = 0;           // number of register values collected.
};

// Struct for memory read information.
struct MRInst {
    UINT32  fnId;              // function id.
    ADDRINT location;          // address location.
    ADDRINT value;             // read value.
    ADDRINT valueSize;         // size of read value in bytes.
    RegInfo  srcRegs[MAX_REGS]; // values in the read registers.
    int regSize = 0;           // number of register values collected.
};

// Structures for holding memory/register reads and writes.
map<ADDRINT,MWInst> writes;     // Write location address to MWInst object.
map<ADDRINT,MRInst> reads;      // Read location address to MRInst object.
map<ADDRINT,MWInst> targetMWs;  // Write location address to MWInst object (Target Insts. Only).
map<int,RegInfo> targetSrcRegs;  // targetSrcRegsKey to RegInfo object.
map<int,RegInfo> targetDesRegs;  // targetDesRegsKey to RegInfo object.

map<int,UINT32> fnCallRet;
int fnCallRetId = 0;

RegInfo srcRegsHolder[MAX_REGS];
RegInfo desRegsHolder[MAX_REGS];
int srcRegSize = 0;
int desRegSize = 0;
UINT8  currentRaxVal[LARGEST_REG_SIZE];
UINT32 currentRaxValSize = 0;
bool populate_regs = false;
ADDRINT lastMemReadLoc = ADDRINT_INVALID;
int targetSrcRegsKey = 0;
int targetDesRegsKey = 0;
bool is_former_range = false;

/**
 * Function: constructModeledIRNode
 * Description: This function analyzes the collected memory/register reads and writes
 *  to construct the JIT compiler IR node model. Then, adds the IR node to the IR graph.
 * Input:
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - system_id (UIN32): JIT compiler system ID.
 * Output: None.
 **/
void constructModeledIRNode(
        UINT32 fnId, UINT8* binary, ADDRINT instSize, 
        UINT32 system_id, ADDRINT addr) {

    // Keep a track of system ID in the IR, if not populated already.
    if (IRGraph->systemId == UINT32_INVALID) {
        IRGraph->systemId = system_id;
    }

    // Create a new node object and populate it with data.
    Node *node = new Node();
    node->id = IRGraph->lastNodeId;

    // Get node address.
    node->intAddress = get_node_address(fnId, system_id);
    assert (node->intAddress != ADDRINT_INVALID);

    // Get opcode of a node.
    ADDRINT *opcode;
    opcode = get_opcode(node, system_id, fnId);
    node->opcode = opcode[0];
    node->opcodeAddress = opcode[1];
    node->id2Opcode[instruction.id] = opcode[0];

    // New node allocation function(s) do "not" always generate nodes.
    // It is true that the function(s) is called to allocate the new node,
    // but the function itself also performs several different checks whether
    // the allocation is really needed or not. If it evaluates (for whatever
    // reason) the allocation is not needed, it simply returns without any
    // node allocation. This we can check by whether the opcode was assigned
    // to node or not. If no opcode was assiggned, then we ignore to construct
    // the node model and return as well.
    if (node->opcode == ADDRINT_INVALID and !node->is_nonIR) {
        return;
    }

    // Get block head address.
    node->blockHead = get_node_block_head(node->intAddress, system_id);
    assert (node->blockHead != ADDRINT_INVALID);

    // Get node size.
    node->size = get_size(node->blockHead, system_id);
    assert (node->size != ADDRINT_INVALID);
    assert (node->size < MAX_NODE_SIZE);

    // Get block tail address.
    node->blockTail = node->blockHead + node->size;
    assert (node->blockTail != ADDRINT_INVALID);

    // Get initial (in)direct value assigned to the node block.
    get_init_block_locs(node, system_id);

    // Add node to IRGraph.
    assert(IRGraph->lastNodeId < MAX_NODES);
    IRGraph->nodes[IRGraph->lastNodeId] = node;
    IRGraph->nodeAddrs[IRGraph->lastNodeId] = node->intAddress;
    IRGraph->lastNodeId += 1;

    // Update the log with CREATE access.
    updateLogInfo(node, addr, fnId, binary, instSize, CREATE);

    // Reset temporary value holders.
    currentRaxValSize = 0;
    targetMWs.clear();
    targetSrcRegs.clear();
    targetDesRegs.clear();
    targetSrcRegsKey = 0;
    targetDesRegsKey = 0;
    is_former_range = false;
}

/**
 * Function: get_node_address
 * Description: This function calls uint8Toaddrint function to retrieve the address of a node
 *  held in the RAX register after returning from the node allocation function.
 * Input:
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - system_id (UINT32): JIT compiler system ID.
 * Output: ADDRINT type address value.
 **/
ADDRINT get_node_address(UINT32 fnId, UINT32 system_id) {

    ADDRINT address = ADDRINT_INVALID;

    assert (currentRaxValSize > 0);

    address = uint8Toaddrint(currentRaxVal, currentRaxValSize);

    assert(address != ADDRINT_INVALID);

    return address;
}

/**
 * Function: get_opcode
 * Description: This function calls appropriate function for each JIT compiler system
 *  to retrieve the opcode of currently forming IR node model.
 * Input:
 *  - node (Node*): Currently forming IR node model.
 *  - system_id (UINT32): JIT compiler system ID.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: Pointer to opcode array.
 **/
ADDRINT *get_opcode(Node *node, UINT32 system_id, UINT32 fnId) {

    static ADDRINT *opcode;

    if (system_id == V8) {
        opcode = get_opcode_v8(node);    
    }
    else if (system_id == JSC) {
        opcode = get_opcode_jsc(node);    
    }
    else if (system_id == SPM) {
        opcode = get_opcode_spm(node, fnId);
    }

    return opcode;
}

/**
 * Function: get_opcode_v8
 * Description: This function analyzes collected memory write information to
 *  retrieve the V8 IR node opcode and address where opcode is stored.
 * Input:
 *  - node (Node*): Currently forming IR node model.
 * Output: Statically allocated opcode array, where index 0 holds opcode and
 * index 1 holds address where opcode is stored within the node block range.
 **/
ADDRINT *get_opcode_v8(Node *node) {

    ADDRINT value = ADDRINT_INVALID;
    map<ADDRINT,MWInst>::iterator it;
    for (it = targetMWs.begin(); it != targetMWs.end(); ++it) {
        if (it->first == node->intAddress) {
            value = (it->second).value;
            break;
        }
    }
    assert(value != ADDRINT_INVALID);

    static ADDRINT opcode[2];
    opcode[0] = ADDRINT_INVALID;
    opcode[1] = ADDRINT_INVALID;

    map<ADDRINT,MWInst>::iterator it2;
    for (it2 = writes.begin(); it2 != writes.end(); ++it2) {
        MWInst write = it2->second;
        if (write.valueSize == V8_OPCODE_SIZE) {
            for (int i = 0; i < write.regSize; i++) {
                if (value == write.srcRegs[i].value) {
                    opcode[0] = write.value;
                    opcode[1] = write.location;
                    break;
                }
            }
            if (opcode[0] != ADDRINT_INVALID) {
                break;
            }
        }
    }

    return opcode;
}

/**
 * Function: get_opcode_jsc
 * Description: This function analyzes collected memory write information to
 *  retrieve the JSC IR node opcode and address where opcode is stored.
 * Input:
 *  - node (Node*): Currently forming IR node model.
 * Output: Statically allocated opcode array, where index 0 holds opcode and
 * index 1 holds address where opcode is stored within the node block range.
 **/
ADDRINT *get_opcode_jsc(Node *node) {

    static ADDRINT opcode[2];
    opcode[0] = ADDRINT_INVALID;
    opcode[1] = ADDRINT_INVALID;

    map<ADDRINT,MRInst>::iterator it;
    for (it = reads.begin(); it != reads.end(); ++it) {
        MRInst read = it->second;
        string fn = strTable.get(read.fnId);
        if (fnInFormers(fn) && read.valueSize == JSC_OPCODE_SIZE) {
            for (int i = 0; i < read.regSize; i++) {
                if (node->intAddress == read.srcRegs[i].value) {
                    opcode[0] = read.value;
                    opcode[1] = read.location;
                    break;
                }
            }
            if (opcode[0] != ADDRINT_INVALID) {
                break;
            }
        }
    }

    return opcode;
}

/**
 * Function: get_opcode_spm
 * Description: This function analyzes collected memory write information to
 *  retrieve the SPM IR node opcode and address where opcode is stored.
 * Input:
 *  - node (Node*): Currently forming IR node model.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: Statically allocated opcode array, where index 0 holds opcode and
 * index 1 holds address where opcode is stored within the node block range.
 **/
ADDRINT *get_opcode_spm(Node *node, UINT32 fnId) {

    static ADDRINT opcode[2];
    opcode[0] = ADDRINT_INVALID;
    opcode[1] = ADDRINT_INVALID;

    string fn = strTable.get(fnId);

    // CFG Block (MBasicBlock) does not hold an opcode, so we analyze the recorded instruction
    // data only if the current node is not a block node. Otherwise, we set is_nonIR to true.
    if (!fnInNonIRAllocs(fn)) {
        map<ADDRINT,MWInst>::iterator it;
        for (it = targetMWs.begin(); it != targetMWs.end(); ++it) {
            MWInst write = it->second;
            if (write.valueSize == SPM_OPCODE_SIZE) {
                for (int i = 0; i < write.regSize; i++) {
                    if ((write.srcRegs[i]).value == node->intAddress) {
                        opcode[0] = write.value;
                        opcode[1] = write.location;
                        break;
                    }
                }
            }
            if (opcode[0] != ADDRINT_INVALID) {
                break;
            }
        }
    }
    else {
        node->is_nonIR = true;
    }

    return opcode;
}

/**
 * Function: get_node_block_head
 * Description: This function returns the node block's head address, where
 *  head means the first memory location of a block.
 * Inputs:
 *  - address (ADDRINT): Address of a node.
 *  - system_id (UINT32): JIT compiler system ID.
 * Output: Address of node block's head.
 **/
ADDRINT get_node_block_head(ADDRINT address, UINT32 system_id) {

    ADDRINT blockHead = ADDRINT_INVALID;

    if (system_id == V8) {
        blockHead = get_node_block_head_v8(address);
    }
    else if (system_id == JSC) {
        // For JSC, block head address is equal to the node address.
        blockHead = address;
    }
    else if (system_id == SPM) {
        // For SpiderMonkey, block head address is equal to the node address.
        blockHead = address;
    }

    return blockHead;
}

/**
 * Function: get_node_block_head_v8
 * Description: This function analyzes the memory writes observed and extracted from
 *  the instructions that belongs to node allocator function to identify the node block's
 *  head address.
 * Input:
 *  - address (ADDRINT): Address of a node.
 * Output: Address of V8 node block's head.
 **/
ADDRINT get_node_block_head_v8(ADDRINT address) {

    ADDRINT regValue = ADDRINT_INVALID;

    map<ADDRINT,MWInst>::iterator it;
    for (it = targetMWs.begin(); it != targetMWs.end(); ++it) {
        MWInst write = it->second;
        if (write.location == address) {
            regValue = write.srcRegs[0].value;
            break;
        }
    }

    return regValue;

}

/**
 * Function: get_init_block_locs
 * Description: This function analyzes the memory write information to extract the values
 *  written between the node's head and tail, where tail is the last memory location of
 *  node block.
 * Input:
 *  - node (Node*): Currently forming IR node model.
 *  - system_id (UINT32): JIT compiler system ID.
 * Output: None.
 **/
void get_init_block_locs(Node *node, UINT32 system_id) {

    int edgeNodeId = INT_INVALID;
    ADDRINT blockHead = node->blockHead;
    ADDRINT blockTail = node->blockTail;

    map<ADDRINT,MWInst>::iterator it;

    for (it = targetMWs.begin(); it != targetMWs.end(); ++it) {
        MWInst write = it->second;
        // If the written location belongs to the node's assigned block, check 2 things:
        // (1) is the value another node? if yes, it's an edge connection.
        // (2) Otherwise, it's a pointer to non-ir object or direct value assignment.
        if (write.location > blockHead && write.location < blockTail) {
            edgeNodeId = compareValuetoIRNodes(write.value);
            if (edgeNodeId != INT_INVALID) {
                assert (node->numberOfEdges < MAX_NODES);
                assert (edgeNodeId < MAX_NODES);
                // set initial node edges.
                node->edgeNodes[node->numberOfEdges] = IRGraph->nodes[edgeNodeId];
                node->edgeAddrs[node->numberOfEdges] = write.location;
                node->numberOfEdges++;
            }
            else if (
                    edgeNodeId == INT_INVALID &&            // write value is non-edge node address
                    write.location != node->intAddress &&   // write value is not node address
                    write.location != node->opcodeAddress   // write value is not node opcode address
            ) {
                bool is_direct = isMemoryWriteLoc(write.value);
                if (is_direct) {
                    // Compute the distance between the block head and the written location,
                    // then write to node's offsets to track which locations are wrriten.
                    ADDRINT offset = write.location - blockHead;
                    assert (node->numberOfLocs < MAX_NODE_SIZE);
                    node->offsets[node->numberOfLocs] = offset;
                    node->valuesInLocs[node->numberOfLocs] = write.value;
                    node->numberOfLocs++;
                }
                else {
                    // TODO: Need to handle indirect (pointing to non-ir object) assignment.
                }
            }
        }
    }
}

/**
 * Function: isMemoryWriteLoc
 * Description: This function compares the passed value (memory address) to the writes
 *  map, which keeps a track of information of all memory writes encountered. If the value
 *  exists in the writes as a key, then the value is a memory location where some value
 *  was written.
 * Input:
 *  - value (ADDRINT): Value to search from the writes map.
 * Output: true if value exists, false otherwise.
 **/
bool isMemoryWriteLoc(ADDRINT value) {

    bool isMemoryWriteLoc = true;

    map<ADDRINT,MWInst>::iterator it;
    it = writes.find(value);
    if (it != writes.end()) {
        isMemoryWriteLoc = false;
    }

    return isMemoryWriteLoc;
}

/**
 * Function: checkRAXValue
 * Description: This function checks the format of RAX value.
 *  This function is needed because the Pin Tool (for some unknown reason)
 *  messes up the byte location,
 *  e.g., Correct format: 00007f25103b2528; Incorrect format: 007f25103b252800.
 * Input:
 *  - value (UINT32*): Value to check the format.
 * Output: true if the value format is correct, false otherwise.
 **/
bool checkRAXValue(UINT8 *value) {

    bool is_valid = true;

    if (value[2] == 0x00) {
        is_valid = false;
    }

    return is_valid;
}

/**
 * Function: uint8Toaddrint
 * Description: This function converts value that is in UINT32 type to ADDRINT type.
 * Input:
 *  - target (UINT8*): Target value to convert to ADDRINT type.
 *  - size (UINT32): Size of the target value.
 * Output: ADDRINT type converted value.
 **/
ADDRINT uint8Toaddrint(UINT8* target, UINT32 size) {
    
    assert (size > 0);

    ADDRINT to = 0;
    for (int i = size-1; i >= 0; i--) {
        to = (to << 8) | target[i];
    }
    
    return to;
}

/**
 * Function: addrintTouint8
 * Description: This function converts value that is in ADDRINT type to UINT32 type.
 * Input:
 *  - target (ADDRINT): Target value to convert to UINT8 type.
 *  - size (UINT32): Size of the target value.
 * Output: UINT32 type converted value.
 **/
UINT8* addrintTouint8(ADDRINT target, UINT32 size) {
    UINT8 *to = new UINT8();
    memcpy(to, &target, size);

    return to;
}

/**
 * Function: uint8Tostring
 * Description: This function converts array of UINT8 type values to string.
 * Input:
 *  - target (ADDRINT): Target value to convert to string type.
 *  - size (UINT32): Size of the target value.
 * Output: String type converted value.
 **/
string uint8Tostring(UINT8* target, ADDRINT size) {
    ostringstream strStream;
    for (int i = size-1; i > 0; i--) {
        strStream << hex << (int)target[i] << " ";
    }
    strStream << hex << (int)target[0];

    return strStream.str();
}

/**
 * Function: compareUINT8
 * Description: This function compares two UINT8 type values.
 * Input:
 *  - target (UINT8*): Target value to compare.
 *  - to (UINT8*): Value to compare Target with.
 *  - size (UINT32): Size of both Target and To (Both must have equal size).
 * Output: true if two are equal, false otherwise.
 **/
bool compareUINT8(UINT8 *target, UINT8 *to, UINT32 size) {

    bool is_equal = true;
    for (UINT32 i = 0; i < size; i++) {
        if (target[i] != to[i]) {
            is_equal = false;
            break;
        }
    }

    return is_equal;
}

/**
 * Function: fnInAllocs
 * Description: This function checks whether the passed function name is a node
 *  block allocator function or not.
 * Input:
 *  - fn (string): Function name string.
 * Output: true if the function is a node block allocator function, false otherwise.
 **/
bool fnInAllocs(string fn) {

    bool is_exists = false;
    for (int i = 0; i < NODE_ALLOC_SIZE; i++) {
        if (NODE_BLOCK_ALLOCATORS[i] == fn) {
            is_exists = true;
            break;
        }
    }

    return is_exists;
}

/**
 * Function: fnInFormers
 * Description: This function checks whether the passed function name is a node
 *  former function or not.
 * Input:
 *  - fn (string): Function name string.
 * Output: true if the function is a node block former function, false otherwise.
 **/
bool fnInFormers(string fn) {

    bool is_exists = false;
    for (int i = 0; i < NODE_FORMERS_SIZE; i++) {
        if (NODE_FORMERS[i] == fn) {
            is_exists = true;
            break;
        }
    }

    return is_exists;
}

/**
 * Function: fnInCreators
 * Description: This function checks whether the passed function name is a node
 *  creator function or not.
 * Input:
 *  - fn (string): Function name string.
 * Output: true if the function is a node creator function, false otherwise.
 **/
bool fnInCreators(string fn) {

    bool is_exists = false;
    for (int i = 0; i < NODE_CREATORS_SIZE; i++) {
        if (MAIN_NODE_CREATORS[i] == fn) {
            is_exists = true;
            break;
        }
    }

    return is_exists;
}

/**
 * Function: fnInNonIRAllocs
 * Description: This function checks whether the passed function name is a non-IR
 *  object allocator or not.
 * Input:
 *  - fn (string): Function name string.
 * Output: true if the function is a non-IR object allocator, flase otherwise.
 **/
bool fnInNonIRAllocs(string fn) {

    bool is_exists = false;
    for (int i = 0; i < NONIR_NODE_ALLOC_SIZE; i++) {
        if (NONIR_NODE_ALLOCATORS[i] == fn) {
            is_exists = true;
            break;
        }
    }

    return is_exists;
}

/**
 * Function: compareValuetoIRNodes
 * Description: This function compare the passed value to node addresses.
 * Output: IR node ID, if value is an IR node address. INT_INVALID value otherwise.
 **/
int compareValuetoIRNodes(ADDRINT value) {

    int node_id = INT_INVALID;

    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        if (value == IRGraph->nodeAddrs[i]) {
            node_id = i;
            break;
        }
    }

    return node_id;
}

/**
 * Function: get_size
 * Description: This function calls appropriate function to get node block size.
 * Input:
 *  - address (ADDRINT): Address of a node to get the block size.
 *  - system_id (UINT32): JIT compiler system ID.
 * Output: Size of node block size.
 **/
ADDRINT get_size(ADDRINT address, UINT32 system_id) {

    ADDRINT size = ADDRINT_INVALID;

    if (system_id == V8) {
        size = get_size_v8(address);
    }
    else if (system_id == JSC) {
        size = get_size_jsc(address);
    }
    else if (system_id == SPM) {
        size = get_size_spm(address);
    }

    assert (size > 0);

    return size;
}

/**
 * Function: get_size_v8
 * Description: This function analyzes register reads collected from the ndoe allocator
 *  function instructions to get node block size. Only for V8.
 * Input:
 *  - address (ADDRINT): Address of a node to get the block size.
 * Output: Size of node block size.
 **/
ADDRINT get_size_v8(ADDRINT address) {

    ADDRINT size = ADDRINT_INVALID;
    RegInfo target;
    bool got_target = false;

    // First, identify the register that holds block memory address.
    map<int,RegInfo>::iterator it;
    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        RegInfo reg = it->second;
        if (reg.instOp == ADD and reg.value == address) {
            target = reg;
            got_target = true;
            break;
        }
    }
    assert (got_target);

    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        RegInfo reg = it->second;
        if (reg.instId == target.instId && reg.value != target.value) {
            size = reg.value;
            break;
        }
    }
    assert (size != ADDRINT_INVALID);

    return size;
}

/**
 * Function: get_size_jsc
 * Description: This function analyzes register reads collected from the ndoe allocator
 *  function instructions to get node block size. Only for JSC.
 * Input:
 *  - address (ADDRINT): Address of a node to get the block size.
 * Output: Size of node block size.
 **/
ADDRINT get_size_jsc(ADDRINT address) {

    ADDRINT size = ADDRINT_INVALID;
    RegInfo target;
    bool got_target = false;

    // First, identify the register that holds block memory address.
    map<int,RegInfo>::iterator it;
    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        RegInfo reg = it->second;
        if (reg.instOp == ADD and reg.value == address) {
            target = reg;
            got_target = true;
            break;
        }
    }
    assert (got_target);

    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        RegInfo reg = it->second;
        if (reg.instId == target.instId && reg.value != target.value) {
            size = reg.value;
            break;
        }
    }
    assert (size != ADDRINT_INVALID);

    return size;
}

/**
 * Function: get_size_jsc
 * Description: This function analyzes register reads collected from the ndoe allocator
 *  function instructions to get node block size. Only for SPM.
 * Input:
 *  - address (ADDRINT): Address of a node to get the block size.
 * Output: Size of node block size.
 **/
ADDRINT get_size_spm(ADDRINT address) {

    ADDRINT size = ADDRINT_INVALID;

    // First, identify the register that holds block memory address.
    map<int,RegInfo>::iterator it;
    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        RegInfo srcReg = it->second;
        if (srcReg.instOp == ADD and srcReg.value == address) {
            map<int,RegInfo>::iterator it2;
            for (it2 = targetDesRegs.begin(); it2 != targetDesRegs.end(); ++it2) {
                RegInfo desReg = it2->second;
                if (desReg.instId == srcReg.instId) {
                    size = desReg.value - srcReg.value;
                    break;
                }
            }
            if (size == ADDRINT_INVALID) {
                for (it2 = targetDesRegs.begin(); it2 != targetDesRegs.end(); ++it2) {
                    RegInfo desReg = it2->second;
                    if (
                            desReg.instOp == srcReg.instOp && 
                            desReg.value - srcReg.value < MAX_NODE_SIZE
                    ) {
                        size = desReg.value - srcReg.value;
                        break;
                    }
                }
            }
            break;
        }
    }

    return size;
}

/**
 * Function: checkCopiedValue
 * Description: Check a value copied by PIN_SafeCopy. If the copied value has
 *  an incorrect format, e.g., 0040032c3bef7f00, where the first byte of '00' is
 *  moved to the end, return false. Otherwise, true.
 * Input:
 *  - value (UINT8*): Value to check the format.
 *  - size (UINT32): Size of value.
 * Output: true if the format is correct, false otherwise.
 **/
bool checkCopiedValue(UINT8 *value, UINT32 size) {

    bool is_correct = true;

    if (size == MEMSIZE) {
        if (value[7] == 0x00 && value[6] != 0x00 && value[0] == 0x00) {
            is_correct = false;
        }
    }

    return is_correct;
}

/**
 * Function: fixCopyValue
 * Description: Fix the value to the correct format if checkCopiedValue returned false
 *  on the input value, e.g., fix 0040032c3bef7f00 to 40032c3bef7f0000.
 * Input:
 *  - buggy (UINT8*): Buggy format value to fix.
 *  - fixed (UINT8*): Fixed value.
 *  - size *UINT32): Size of value.
 * Output: None.
*/
void fixCopyValue(UINT8 *buggy, UINT8 *fixed, UINT32 size) {

    assert (sizeof(buggy) == size);

    // Copy only the 6 bytes between the first and last 00s.
    UINT32 fix_idx = 0;
    for (UINT32 buggy_idx = 1; buggy_idx < size-1; buggy_idx++) {
        assert (buggy_idx < sizeof(buggy));
        fixed[fix_idx] = buggy[buggy_idx];
        fix_idx++;
    }
    // Manually add two 00s at the end of the fix array.
    fixed[fix_idx+1] = 0x00;
    fixed[fix_idx+2] = 0x00;
}

/**
 * Function: getEdgeIdx
 * Description: This function returns the index of node's edge where the edge
 *  node address is stored.
 * Input:
 *  - node (Node*): The node, which holds the edge.
 *  - Address (ADDRINT): Address of a node, which it's connected to the edge.
 * Output: Edge index, if exists. INT_INVALID otherwise.
 **/
int getEdgeIdx(Node *node, ADDRINT address) {

    int edge_idx = INT_INVALID;

    for (int i = 0; i < node->numberOfEdges; i++) {
        if (address == node->edgeAddrs[i]) {
            edge_idx = i;
            break;
        }
    }

    return edge_idx;
}

/**
 * Function: get_updated_opcode
 * Description: This function calls appropriate system-specific function to update node's opcode
 *  if optimizer performs optimization.
 * Input:
 *  - node (Node*): Node to update opcode.
 *  - location (ADDRINT): Location address between the node block range, where opcode is stored.
 *  - value (ADDRINT): value, which is expected to be opcode.
 *  - valueSize (ADDRINT): Size of value.
 *  - system_id (UINT32): JIT compiler system ID.
 * Output: Pointer to opcode array.
 **/
ADDRINT *get_updated_opcode(Node *node, ADDRINT location, ADDRINT value, ADDRINT valueSize, UINT32 system_id) {

    static ADDRINT *opcode;

    if (system_id == V8) {
        opcode = get_update_opcode_v8(node, value);
    }
    else if (system_id == JSC) {
        opcode = get_update_opcode_jsc(node, location, value, valueSize);
    }
    else if (system_id == SPM) {
        opcode = get_update_opcode_spm(node, location, value, valueSize);
    }

    return opcode;
}

/**
 * Function: get_update_opcode_v8
 * Description: This function checks whether the value is an opcode or node.
 *  Then, updates node's opcode, if the value is opcode.
 * Input:
 *  - node (Node*): Node to update opcode.
 *  - value (ADDRINT): value, which is expected to be opcode.
 * Output: Statically allocated opcode array, where index 0 holds opcode and
 * index 1 holds address where opcode is stored within the node block range.
 **/
ADDRINT *get_update_opcode_v8(Node *node, ADDRINT value) {

    static ADDRINT opcode[2];
    opcode[0] = ADDRINT_INVALID;
    opcode[1] = ADDRINT_INVALID;

    map<ADDRINT,MWInst>::iterator it;
    for (it = writes.begin(); it != writes.end(); ++it) {
        MWInst write = it->second;
        // If the value has the same size as the V8's opcode size, analyze further.
        if (write.valueSize == V8_OPCODE_SIZE) {
            for (int i = 0; i < write.regSize; i++) {
                if (value == write.srcRegs[i].value) {
                    opcode[0] = write.value;
                    opcode[1] = write.location;
                    break;
                }
            }
            if (opcode[0] != ADDRINT_INVALID) {
                break;
            }
        }
    }

    return opcode;
}

/**
 * Function: get_update_opcode_jsc
 * Description: This function checks whether the value is an opcode or node.
 *  Then, updates node's opcode, if the value is opcode.
 * Input:
 *  - node (Node*): Node to update opcode.
 *  - location (ADDRINT): Location address between the node block range, where opcode is stored.
 *  - value (ADDRINT): value, which is expected to be opcode.
 *  - valueSize (ADDRINT): Size of value.
 * Output: Statically allocated opcode array, where index 0 holds opcode and
 * index 1 holds address where opcode is stored within the node block range.
 **/
ADDRINT *get_update_opcode_jsc(Node *node, ADDRINT location, ADDRINT value, ADDRINT valueSize) {

    static ADDRINT opcode[2];
    opcode[0] = ADDRINT_INVALID;
    opcode[1] = ADDRINT_INVALID;

    // FOR JSC, opcode address doesn't change.
    if (location == node->opcodeAddress && valueSize == JSC_OPCODE_SIZE) {
        opcode[0] = value;
        opcode[1] = location;
    }

    return opcode;
}

/**
 * Function: get_update_opcode_spm
 * Description: This function checks whether the value is an opcode or node.
 *  Then, updates node's opcode, if the value is opcode.
 * Input:
 *  - node (Node*): Node to update opcode.
 *  - location (ADDRINT): Location address between the node block range, where opcode is stored.
 *  - value (ADDRINT): value, which is expected to be opcode.
 *  - valueSize (ADDRINT): Size of value.
 * Output: Statically allocated opcode array, where index 0 holds opcode and
 * index 1 holds address where opcode is stored within the node block range.
 **/
ADDRINT *get_update_opcode_spm(Node *node, ADDRINT location, ADDRINT value, ADDRINT valueSize) {

    static ADDRINT opcode[2];
    opcode[0] = ADDRINT_INVALID;
    opcode[1] = ADDRINT_INVALID;

    // FOR JSC, opcode address doesn't change.
    if (location == node->opcodeAddress && valueSize == SPM_OPCODE_SIZE) {
        opcode[0] = value;
        opcode[1] = location;
    }

    return opcode;
}

// Functions for prints for debugging.

/**
 * Function: printUINT8
 * Description: This function prints UINT8 type value in format.
 * Input:
 *  - arr (UINT8*): UINT8 type value array.
 *  - size (UINT32): Size of value.
 * Output: None.
 **/
void printUINT8(UINT8* arr, UINT32 size) {

    assert (size > 0);

    for (int i = size-1; i > 0; i--) {
        printf("%02x ", arr[i]);
    }
    printf("%02x\n", arr[0]);
}

// =============================================================

/**
 * Function: recordFnCallRet
 * Description: Records function call-return sequences. The recording starts from the
 * first JIT compiler execution instruction.
 * Input:
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: None.
 */
void recordFnCallRet(UINT32 fnId) {

    if (fnCallRetId == 0 && fnId != 0) {
        fnCallRet[fnCallRetId] = fnId;
        fnCallRetId++;
    }
    else {
        assert(fnCallRetId >= 0);
        map<int,UINT32>::iterator it;
        it = fnCallRet.find(fnCallRetId);
        if (fnCallRet[fnCallRetId-1] != fnId && fnId != 0 && it == fnCallRet.end()) {
            fnCallRet[fnCallRetId] = fnId;
            fnCallRetId++;
        }
    }
}

/**
 * Function: recordSrcRegs
 * Description: Records information on source registers of an instruction.
 * Input:
 *  - tid (THREADID): Thread ID struct object.
 *  - ctx (CONTEXT*): A structure that keeps architectural state of the processor.
 *  - srcRegs (RegVector*): Source register information holder.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - instOpcode (UINT32): Instruction opcode.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordSrcRegs(
        THREADID tid, const CONTEXT *ctx, const RegVector *srcRegs, UINT32 fnId,
        UINT32 instOpcode) {

    // In case srcRegSize was not reset, reset it to zero.
    srcRegSize = 0;

    UINT8 buf[LARGEST_REG_SIZE];
    for(UINT8 i = 0; i < srcRegs->getSize(); i++) {
        LynxReg lReg = srcRegs->at(i);
        LynxReg fullLReg = fullLynxReg(lReg);
        UINT32 fullSize = lynxRegSize(fullLReg);
        REG reg = LynxReg2Reg(fullLReg);
        PIN_GetContextRegval(ctx, reg, buf);

        UINT8 RegValue[fullSize];
        PIN_SafeCopy(RegValue, buf, fullSize);

        ADDRINT regValueInt = uint8Toaddrint(RegValue, fullSize);

        // Create a new srcRegs object.
        RegInfo srcReg;
        srcReg.instId = instruction.id;
        srcReg.fnId = fnId;
        srcReg.instOp = instOpcode;
        srcReg.lReg = fullLReg;
        srcReg.value = regValueInt;

        assert (srcRegSize < MAX_REGS);
        srcRegsHolder[srcRegSize] = srcReg;
        srcRegSize++;
    }
}

/**
 * Function: recordDestRegs
 * Description: Records the instruction's destination registers and flags in thread local storage so
 *  that they can be printed after the instruction executes.
 * Input:
 *  - tid (THREADID): Thread ID struct object.
 *  - destRegs (RegVector*): Destination register information vector.
 *  - destFalgs (UINT32): Destination register flag.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordDestRegs(THREADID tid, const RegVector *destRegs, UINT32 destFlags) {
    ThreadData &data = tls[tid];
    data.destFlags = destFlags;
    data.destRegs = destRegs;
}

/**
 * Function: checkMemRead
 * Description: This function needs to be executed regardless of whether we are writing memory reads 
 *  into the trace. It checks to make sure we've already seen a memory value at this address. If not,
 *  it needs to be recorded in the trace (since it won't be in the reader's shadow architecture). 
 *  It also checks to see if we are currently looking at a special memory region that gets updated by
 *  the kernel. In this case, we won't know if we saw the current value before so we also must always
 *  print out those values.
 * Input:
 *  - readAddr (ADDRINT): Memory read location address.
 *  - readSize (UINT32): Size of readAddr.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: None
 **/
void checkMemRead(ADDRINT readAddr, UINT32 readSize, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr) {

    string fn = strTable.get(fnId);

    UINT8 value[MAX_MEM_OP_SIZE];
    PIN_SafeCopy(value, (UINT8 *) (readAddr), readSize);

    ADDRINT valueInt = uint8Toaddrint(value, readSize);

    // Create new object to hold memory read information.
    MRInst read;
    read.fnId = fnId;
    read.location = readAddr;
    read.value = valueInt;
    read.valueSize = readSize;
    // Store it in the reads map.
    assert(reads.size()+1 < reads.max_size());
    reads[readAddr] = read;
    // Track the last added key.
    lastMemReadLoc = readAddr;
    // Mark that the tool needs to update the register.
    populate_regs = true;

    // Check if the current memory location belongs to any of existing node,
    // i.e., node evaluation.
    nodeEvaluation(readAddr, valueInt, fnId, binary, instSize, addr);

    PIN_MutexUnlock(&dataLock);
}

/**
 * Function: check2MemRead
 * Description: This function simply calls checkMemRead twice.
 * Input:
 *  - readAddr1 (ADDRINT): Address of first memory read location address.
 *  - readAddr2 (ADDRINT): Address of second memory read location address.
 *  - readSize (UINT32): Size of memory reads.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: None.
 **/
void check2MemRead(
        ADDRINT readAddr1, ADDRINT readAddr2, UINT32 readSize, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr)
{
    checkMemRead(readAddr1, readSize, fnId, binary, instSize, addr);
    checkMemRead(readAddr2, readSize, fnId, binary, instSize, addr);
}

/**
 * Function: recordMemWrite
 * Description: We can only get the address of a memory write from PIN before an instruction
 * executes. Since we need this information after the instruction executes (so we can get the new
 * values), we need to record the address and size of the memory write in thread local storage so
 * we can get the information later.
 * Input:
 *  - tid (THREADID): Thread ID struct object.
 *  - addr (ADDRINT): Memory write location address.
 *  - size (UINT32): Size of memory write value.
 * Output: None
 **/
void recordMemWrite(THREADID tid, ADDRINT addr, UINT32 size) {
    // For a memory write all we need to do is record information about it
    // so after the instruction we can print it
    ThreadData &data = tls[tid];
    data.memWriteAddr = addr;
    data.memWriteSize = size;

    PIN_MutexLock(&dataLock);
    mem.setSeen(addr, size);
    PIN_MutexUnlock(&dataLock);
}

/**
 * Function: analyzeRecords
 * Description: This function analyzes all the recorded information for the instruction.
 * Input:
 *  - tid (THREADID): Thread ID struct object.
 *  - ctx (CONTEXT*): A structure that keeps architectural state of the processor.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - opcode (UINT32): Instruction opcode.
 *  - is_create(bool): Flag indicating whether the current instruction belongs node creator
 *  function or not.
 *  - system_id (UINT32): JIT compiler system ID.
 * Output: true if labeled, false otherwise.
 **/
bool analyzeRecords(
        THREADID tid, const CONTEXT *ctx, UINT32 fnId, 
        UINT32 opcode, bool is_create, UINT8* binary, 
        ADDRINT instSize, UINT32 system_id, ADDRINT addr
) {

    ThreadData &data = tls[tid];

    // Check if current instruction is within the node creation range.
    // If true, set the is_former_range, which is the variable to keep track of the range, to true.
    if (!is_former_range && is_create) {
        is_former_range = true;
    }

    // Retrieve the function name.
    string fn = strTable.get(fnId);

    // If the instruction has register write, then analyze register write, e.g., W:RAX=...
    // Note: We only care about the last RAX register value of node allocator
    // function instructions.
    if(fnInAllocs(fn) && data.destRegs != NULL) {
        analyzeRegWrites(tid, ctx, fnId, opcode);
        data.destRegs = NULL;
    }

    // Keep tracks of source register information of the node allocator function instructions
    // separately.
    if (is_former_range || fnInAllocs(fn)) {
        for (int i = 0; i < srcRegSize; i++) {
            assert (i < MAX_REGS);
            RegInfo srcReg = srcRegsHolder[i];
            assert(targetSrcRegs.size()+1 < targetSrcRegs.max_size());
            targetSrcRegs[targetSrcRegsKey] = srcReg;
            targetSrcRegsKey++;
        }
        for (int j = 0; j < desRegSize; j++) {
            assert (j < MAX_REGS);
            RegInfo desReg = desRegsHolder[j];
            assert(targetDesRegs.size()+1 < targetDesRegs.max_size());
            targetDesRegs[targetDesRegsKey] = desReg;
            targetDesRegsKey++;
        }
    }

    // If the instruction has memory write, analyze memory write, e.g., MW[..]=...
    if(data.memWriteSize != 0) {
        analyzeMemWrites(tid, fnId, is_former_range, binary, instSize, system_id, addr);
        data.memWriteSize = 0; 
    }

    // If the instruction has memory read, populate the source register
    // information to the MR tracker object.
    if (populate_regs) {
        for (int i = 0; i < srcRegSize; i++) {
            assert(i < MAX_REGS);
            reads[lastMemReadLoc].srcRegs[i] = srcRegsHolder[i];
        }
        reads[lastMemReadLoc].regSize = srcRegSize;
        populate_regs = false;
    }

    memset(srcRegsHolder, 0, srcRegSize);
    srcRegSize = 0;
    memset(desRegsHolder, 0, desRegSize);
    desRegSize = 0;

    PIN_MutexLock(&traceLock);
    data.eventId = eventId;
    eventId++;

    // Update instruction id
    instruction.id++;

    PIN_MutexUnlock(&traceLock);

    //mark that we printed the instruction
    bool labeled = false;
    data.print = false;
    return labeled;
}

/**
 * Function: analyzeRegWrites
 * Description: This function analyzes register writes of the current instruction.
 * Input:
 *  - tid (THREADID): Thread ID struct object.
 *  - ctx (CONTEXT*): A structure that keeps architectural state of the processor.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - opcode (UINT32): Instruction opcode.
 * Output: None.
 **/
void analyzeRegWrites(THREADID tid, const CONTEXT *ctx, UINT32 fnId, UINT32 opcode) {
    
    ThreadData &data = tls[tid];

    desRegSize = 0;

    REG reg;
    UINT8 buf[LARGEST_REG_SIZE];
    const RegVector *writeRegs = data.destRegs;
    for(UINT8 i = 0; i < writeRegs->getSize(); i++) {
        LynxReg lReg = writeRegs->at(i);
        LynxReg fullLReg = fullLynxReg(lReg);
        UINT32 fullSize = lynxRegSize(fullLReg);
        reg = LynxReg2Reg(fullLReg);
        PIN_GetContextRegval(ctx, reg, buf);

        string LynRegStr = LynxReg2Str(fullLReg);
        // Update currentRaxVal.
        if (LynRegStr == TARGET_REG) {
            UINT8 RAXValue[fullSize];
            PIN_SafeCopy(RAXValue, buf, fullSize);

            if (checkRAXValue(RAXValue)) {
                assert (fullSize > 0);
                PIN_SafeCopy(currentRaxVal, buf, fullSize);
                currentRaxValSize = fullSize;
            }
        }

        UINT8 RegValue[fullSize];
        PIN_SafeCopy(RegValue, buf, fullSize);

        ADDRINT regValueInt = uint8Toaddrint(RegValue, fullSize);

        // Create a new desRegs object.
        RegInfo desReg;
        desReg.instId = instruction.id;
        desReg.fnId = fnId;
        desReg.instOp = opcode;
        desReg.lReg = fullLReg;
        desReg.value = regValueInt;

        assert (desRegSize < MAX_REGS);
        desRegsHolder[desRegSize] = desReg;
        desRegSize++;
    }
}

/**
 * Function: analyzeMemWrites
 * Description: This function analyzes the memory writes of current instruction.
 *  In addition, it calls trackOptimization function to further analyze the memory writes
 *  to seek for potential optimization event happeneing to the IR.
 * Input:
 *  - tid (THREADID): Thread ID struct object.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - is_range (bool): Boolean flag to indicate whether the currently analyzing instruction
 *    is within the range of node formation or not.
 *  - system_id (UINT32): JIT compiler system ID. 
 * Output: None.
 **/
void analyzeMemWrites(
        THREADID tid, UINT32 fnId, bool is_range, UINT8* binary,
        ADDRINT instSize, UINT32 system_id, ADDRINT addr) 
{

    ThreadData &data = tls[tid];

    string fn = strTable.get(fnId);

    UINT8 value[data.memWriteSize];
    PIN_SafeCopy(value, (UINT8 *) (data.memWriteAddr), data.memWriteSize);

    // Below code is to fix the bug (likely) caused by the Pin tool.
    // In some cases, bits of the value are off, e.g., 00007f is returned in 007f00.
    // Thus, we are currently fixing the bits manually.
    ADDRINT valueInt = ADDRINT_INVALID;
    if (data.memWriteSize == MEMSIZE) {
        bool is_correct = checkCopiedValue(value, data.memWriteSize);
        if (!is_correct) {
            UINT8 fixed[data.memWriteSize];
            fixCopyValue(value, fixed, data.memWriteSize);
            valueInt = uint8Toaddrint(fixed, data.memWriteSize);
        }
        else {
            valueInt = uint8Toaddrint(value, data.memWriteSize);
        }
    }
    else {
        // Convert value (UINT8) to ADDRINT type value.
        valueInt = uint8Toaddrint(value, data.memWriteSize);
    }

    // Create a new object to hold the memory write and register information of the instruction.
    MWInst write;
    write.fnId = fnId;
    write.location = data.memWriteAddr;
    write.value = valueInt;
    write.valueSize = data.memWriteSize;
    for (int i = 0; i < srcRegSize; i++) {
        assert (i < MAX_REGS);
        write.srcRegs[i] = srcRegsHolder[i];
    }
    write.regSize = srcRegSize;

    // Add the 'write' object to the 'writes' map.
    assert(writes.size()+1 < writes.max_size());
    writes[data.memWriteAddr] = write;

    // Collect location and value in MWs only in the node former function instructions.
    if (is_range) {
        assert(targetMWs.size()+1 < targetMWs.max_size());
        targetMWs[data.memWriteAddr] = write;
    }

    // If there are IR nodes, analyzes and extracts, if exists, optimization information.
    if (IRGraph->lastNodeId > 0) {
        trackOptimization(
                write.location, write.value, 
                data.memWriteSize, fnId,
                binary, instSize, system_id, addr);
    }
}

/**
 * Function: trackOptimization
 * Description: This function analyze the passed information extracted from the instruction to
 *  identify whether the instruction is an event of IR optimization. If it is, then the function
 *  updates the IR model and the optimization history holder to keep a record of optimization.
 *  This function is called only in the analyzeMemWrites function.
 * Input:
 *  - location (ADDRINT): Memory location where value is being written to.
 *  - value (ADDRINT): Value that is being written to the location.
 *  - valueSize (ADDRINT): Size of the written value.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - system_id (UINT32): JIT compiler system ID. 
 * Output: None.
 **/
void trackOptimization(
        ADDRINT location, ADDRINT value, ADDRINT valueSize, 
        UINT32 fnId, UINT8* binary, ADDRINT instSize, 
        UINT32 system_id, ADDRINT addr) 
{

    // Check if the current memory location belongs to any one of existing node.
    ADDRINT nodeId = ADDRINT_INVALID;
    bool is_node_addr = false;
    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        Node *node = IRGraph->nodes[i];
        if (location >= node->blockHead && location < node->blockTail) {
            // If the writing location is within the range of one node, then get the node id.
            nodeId = node->id;
            // If the writing location is the address of a node, then mark 'is_node_addr' to true.
            if (location == node->intAddress) {
                is_node_addr = true;
            }
            break;
        }
    }

    // If the location belongs to one of the nodes,
    if (nodeId != ADDRINT_INVALID) {
        Node *node = IRGraph->nodes[nodeId];
        // Check if the memory write value is an address of a node.
        int value_id = compareValuetoIRNodes(value);

        // Check if the location is already occupied edge.
        int edge_idx = getEdgeIdx(node, location);
        // If the location is an already occupied edge, handle edge removal or edge replace.
        if (edge_idx != INT_INVALID) {
            // If value is '0', which is to wipe out the memory location, handle edge 'removal'.
            if (value == WIPEMEM) {
                edgeRemoval(node, edge_idx, fnId, binary, instSize, addr);
            }
            // If value is one of the existing nodes, handle edge 'replace'.
            else if (value_id != INT_INVALID) {
                edgeReplace(node, value_id, edge_idx, fnId, binary, instSize, addr);
            }
        }
        // If the location is not occupied edge, but the value is a node, handle edge 'addition'.
        else if (edge_idx == INT_INVALID && value_id != INT_INVALID) {
            edgeAddition(node, location, value_id, fnId, binary, instSize, addr);
        }
        // If the location belongs to some node block, but it's not an edge and the value is not a node.
        else if (edge_idx == INT_INVALID && value_id == INT_INVALID) {
            // Node's are being destroyed and the location is being wiped by writing '0'.
            // We check such pattern in the instruction and mark the node dead.
            if (is_node_addr && value == WIPEMEM) {
                nodeDestroy(node, fnId, binary, instSize, addr);
            }
            // If the write is happening at node address locaiton, then check whether it's an opcode
            // update or not and update the opcode, if yes.
            else if (is_node_addr && value != WIPEMEM) {
                opcodeUpdate(node, location, value, valueSize, system_id, fnId, binary, instSize, addr);
            }
            // If the write is happening at non-node address location, then check for
            // the value assignment (direct & indirect).
            else if (!is_node_addr) {
                bool is_direct = isMemoryWriteLoc(value);
                // TODO: 'valueSize < 8' is to prevent considering the memory address is considered as
                // a value with an assmption is that the address's size is 8. This is not a good approach
                // so we need to update it with more appropriate way.
                if (is_direct && valueSize < 8) {
                    directValueWrite(node, location, value, fnId, binary, instSize, addr); 
                }
                else {
                    // TODO: Need to handle indirect (pointing to non-ir object) assignment.
                }
            }
        }
    }
}

/**
 * Function: updateLogInfo
 * Description: This function keeps track of the optimization and access history (log).
 *  It updates the history holder of both in the node and IR graph models.
 * Input:
 *  - node (Node*): IR node that was accessed.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 *  - accesType (Access): Type of access to the node.
 * Output: None.
 **/
void updateLogInfo(Node *node, ADDRINT addr, UINT32 fnId, UINT8* binary, ADDRINT instSize, Access accessType) {

    // Create a new instInfo object and update it.
    InstInfo instInfo;
    instInfo.address = addr;
    instInfo.fnCallRetId = fnCallRetId;
    instInfo.fnId = fnId;
    instInfo.binary = new UINT8[instSize];
    memcpy(instInfo.binary, binary, instSize);
    instInfo.instSize = instSize;
    instInfo.accessType = accessType;

    // Update the node's function info. map.
    assert((node->instInfo).size()+1 < (node->instInfo).max_size());
    node->instInfo[instruction.id] = instInfo;
    node->lastInfoId = instruction.id;
}

/**
 * Function: isSameAccess
 * Description: This function checks whether the currently observed node access is the same
 *  as the last node access (the same node and same access type) or not.
 * Input:
 *  - node (Node*): IR node that was accessed.
 *  - fnInfo (instInfo): Information of the function and access type to the node.
 * Output: true if same, false otherwise.
 **/
bool isSameAccess(Node *node, InstInfo instInfo) {

    bool is_same = false;
    
    // Get the node's last fnInfo.
    InstInfo latest = node->instInfo[node->lastInfoId];

    // If the currently analyzing fnInfo's fnId and accessType are
    // equal to the node's last fnInfo, set is_same to true.
    // Otherwise, false.
    if (
            latest.fnCallRetId == instInfo.fnCallRetId
            && latest.accessType == instInfo.accessType

    ) {
        is_same = true;
    }
    else {
        is_same = false;
    }

    return is_same;
}

/**
 * Function: edgeRemoval
 * Description: This function removes a node from edge in the IR model if edge removal
 *  optimization event's been observed. Then, it updates the edge owner node's history.
 * Input:
 *  - node (Node*): Owner node of the edge, which removal optimization was performed.
 *  - edge_idx (int): Index of the edge that is target to be removed in the model.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output:
 **/
void edgeRemoval(Node *node, int edge_idx, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr) {

    Node *target = node->edgeNodes[edge_idx];
    // In some cases, I see an instruction that cleans memory location where it's alreay
    // wiped out with '0' and never written with other value. Such instruction will clean
    // the node's edge (already to get NULL). Thus, the earlier line will return NULL.
    // We only count the edge removal only if the edge has a valid node, so we handle
    // this case by checking that the returned value from the earlier line is NOT NULL.
    if (target != NULL) {
        // Update node's 'remove' optimization information.
        assert((node->instOrder2remNodeId).size()+1 < (node->instOrder2remNodeId).max_size());
        node->instOrder2remNodeId[instruction.id] = target->id;
        // Set the removed edge to NULL.
        assert(edge_idx < node->numberOfEdges);
        node->edgeNodes[edge_idx] = NULL;
        // Update function log information.
        updateLogInfo(node, addr, fnId, binary, instSize, REMOVAL);
    }
}

/**
 * Function: edgeReplace
 * Description: This function replaces nodes from edge in the IR model if edge replacement
 *  optimization event's been observed. Then, it updates the edge owner node's history.
 * Input:
 *  - node (Node*): Owner node of the edge, which replacement optimization was performed.
 *  - value_id (int):  Value ID is equal to one of the node's ID in the IR model.
 *  - edge_idx (int): Index of the edge that is target to be replaced in the model.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: None.
 **/
void edgeReplace(Node *node, int value_id, int edge_idx, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr) {

    assert(value_id < IRGraph->lastNodeId);
    Node *to = IRGraph->nodes[value_id];
    ReplacedInfo replacedInfo;
    // Update node's 'replace' optimization information.
    if (node->edgeNodes[edge_idx] != NULL) {
        assert(edge_idx < node->numberOfEdges);
        Node *from = node->edgeNodes[edge_idx];
        replacedInfo.nodeIdFrom = from->id;
        replacedInfo.nodeIdTo = to->id;
    }
    else {
        replacedInfo.nodeIdFrom = INT_INVALID;
        replacedInfo.nodeIdTo = to->id;
    }
    assert((node->instOrder2repInfo).size()+1 < (node->instOrder2repInfo).max_size());
    node->instOrder2repInfo[instruction.id] = replacedInfo;
    // Replace existing edge node with the node with 'value_id'.
    assert(edge_idx < node->numberOfEdges);
    node->edgeNodes[edge_idx] = to;
    // Update function log information.
    updateLogInfo(node, addr, fnId, binary, instSize, REPLACE);
}

/**
 * Function: edgeAddition
 * Description: This function adds new edge to a node if edge additiion optimization event's
 *  been optimization. Then, it updates the edge owner node's history.
 * Input:
 *  - node (Node*): Owner node of the edge, which add optimization was performed.
 *  - location (ADDRINT): Address of location where the value is written.
 *  - value_id (int):  Value ID is equal to one of the node's ID in the IR model.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: None.
 **/
void edgeAddition(Node *node, ADDRINT location, int value_id, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr) {

    assert(value_id < IRGraph->lastNodeId);
    Node *adding = IRGraph->nodes[value_id];
    // Update node's 'add' optimization information.
    assert((node->instOrder2addNodeId).size()+1 < (node->instOrder2addNodeId).max_size());
    node->instOrder2addNodeId[instruction.id] = adding->id;
    // Add an edge from 'this' node to the node with 'value_id'. 
    assert (node->numberOfEdges < MAX_NODES);
    node->edgeNodes[node->numberOfEdges] = adding;
    node->edgeAddrs[node->numberOfEdges] = location;
    node->numberOfEdges++;
    // Update function log information.
    updateLogInfo(node, addr, fnId, binary, instSize, ADDITION);
}

/**
 * Function: nodeDestroy
 * Description: This function set node's alive field to false if node destroy optimizaion
 *  event's been observed. Then, it updates the destroyed node's history.
 * Input:
 *  - node (Node*): Destroyed node.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: None.
 **/
void nodeDestroy(Node *node, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr) {

    // Simply set node->alive to false to indicate the node's been destroyed.
    node->alive = false;
    // Update function log information.
    updateLogInfo(node, addr, fnId, binary, instSize, KILL);
}

/**
 * Function: directValueWrite
 * Description: This function observes value writing to a node and updates the node model,
 *  which the value was written. Note that this function only handle direct write.
 * Input:
 *  - node (Node*): Node where the value's been written.
 *  - location (ADDRINT): Address where the value is been written.
 *  - value (ADDRINT): Written value.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output: None.
 **/
void directValueWrite(Node *node, ADDRINT location, ADDRINT value, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr) {

    // Compute the offset first.
    ADDRINT offset = location - node->blockHead;
    // Create a new DirectValOpt object.
    DirectValOpt directValOpt;
    directValOpt.offset = offset;

    // Check if the offset already occupied with some value.
    bool is_written = false;
    for (int i = 0; i < node->numberOfLocs; i++) {
        // If the offset is already a written location, then update the value
        // and mark the is_written to true.
        if (node->offsets[i] == offset) {
            assert (i < MAX_NODE_SIZE);
            // Update the directValOpt's valFrom, valTo, and is_update.
            directValOpt.valFrom = node->valuesInLocs[i];
            directValOpt.valTo = value;
            directValOpt.is_update = true;
            // Update the value.
            node->valuesInLocs[i] = value;
            is_written = true;
            break;
        }
    }
    // If is_written is false and location is not a location for opcode,
    // then is a new value write. Thus, add the offset and value.
    if (!is_written && location != node->opcodeAddress) {
        // Update the directValOpt's valTo.
        directValOpt.valTo = value;
        // Update offsets and valuesInLocs.
        assert(node->numberOfLocs < MAX_NODE_SIZE);
        node->offsets[node->numberOfLocs] = offset;
        node->valuesInLocs[node->numberOfLocs] = value;
        node->numberOfLocs++;
        is_written = true;
    }

    if (is_written) {
        assert((node->instOrder2dirValOpt).size()+1 < (node->instOrder2dirValOpt).max_size());
        node->instOrder2dirValOpt[instruction.id] = directValOpt;
        // Update function log information.
        updateLogInfo(node, addr, fnId, binary, instSize, VALUE_CHANGE);
    }
}

/**
 * Function: opcodeUpdate
 * Description: This function updates the node's opcode if opcode update optimization
 *  has been observed. Then, it updates the node's history.
 * Input:
 *  - node (Node*): Node that opcode is been updated.
 *  - location (ADDRINT): Address where value is written.
 *  - value (ADDRINT): Value written. Likely to be a new opcode.
 *  - valueSize (ADDRINT): Size of value.
 *  - system_id (UINT32): JIT compiler system ID.
 *  - fnId (UINT32): ID of a function that currently analyzing instruction belongs to.
 * Output:
 **/
void opcodeUpdate(
        Node *node, ADDRINT location, ADDRINT value, ADDRINT valueSize, 
        UINT32 system_id, UINT32 fnId, UINT8* binary, ADDRINT instSize,
        ADDRINT addr) 
{

    // Check if the memory write value is an opcode.
    ADDRINT *opcode;
    opcode = get_updated_opcode(node, location, value, valueSize, system_id);

    // If update opcode value exists and the value is not equal to the current node's opcode,
    // update the node's opcode and information tracking variables.
    if (opcode[0] != ADDRINT_INVALID && opcode[0] != node->opcode) {
        // We don't update the main opcode.
        // We track only the optimization (update) information.
        // Update opcode update information.
        assert((node->id2Opcode).size()+1 < (node->id2Opcode).max_size());
        node->id2Opcode[instruction.id] = opcode[0];
        // Update function log information.
        updateLogInfo(node, addr, fnId, binary, instSize, OP_UPDATE);
    }
}

/**
 * Function: nodeEvaluation
 * Description: This function identifies whether the address that the instruction performs memory
 *  read belong to any existing node or not. If it does, it extracts the information and stores it to
 *  the node.
 * Input:
 *  - readAddr (ADDRINT): memory address where memory read happens.
 *  - valueInt (ADDRINT): value in the location.
 *  - fnId (UINT32): function id.
 *  - binary (UINT8*): instruction binary, i.e., opcode & operands.
 *  - instSize (ADDRINT): size of the instruction.
 *  Output:
 **/
void nodeEvaluation(ADDRINT readAddr, ADDRINT valueInt, UINT32 fnId, UINT8* binary, ADDRINT instSize, ADDRINT addr) {
    ADDRINT nodeId = ADDRINT_INVALID;
    bool is_node_block = false;
    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        Node *node = IRGraph->nodes[i];
        // Check if the memory read access happens to some node block.
        // readAddr: Location where reading the value from.
        // valueInt: Value read from the readAddr (location).
        if (
                (readAddr >= node->blockHead && readAddr < node->blockTail) ||
                (valueInt >= node->blockHead && valueInt < node->blockTail)
        ) {
            nodeId = node->id;
            is_node_block = true;
            // Compute the accessed (evaluated) node offset.
            ADDRINT offset = ADDRINT_INVALID;
            if (readAddr >= node->blockHead && readAddr < node->blockTail) {
                offset = readAddr - node->blockHead;
            }
            else {
                offset = valueInt - node->blockHead;
            }
            assert (offset != ADDRINT_INVALID);
            // Update the node's instOrder2offset with the computed offset.
            Offset2Value offset2value;
            offset2value.offset = offset;

            // THIS MAY HURT THE PERFORMACE SIGNIFICANTLY. REMOVE IF IT DOES.
            map<ADDRINT,MWInst>::iterator it;
            it = writes.find(valueInt);
            if (it != writes.end()) {
                offset2value.value = writes[valueInt].value;
            }
            else {
                offset2value.value = valueInt;
            }

            (IRGraph->nodes[i])->instOrder2offVal[instruction.id] = offset2value;
            break;
        }
    }
    // If the access was happened, update the node's access log.
    if (is_node_block) {
        Node *accessed =  IRGraph->nodes[nodeId];
        updateLogInfo(accessed, addr, fnId, binary, instSize, EVALUATE);
    }
}

/**
 * Function: getFileBuf
 * Description: Get a buffer for the file that is guaranteed to fit the given size. Must provide the file's
 *  buffer and the current position in that buffer. 
 * Side Effects: Writes to file if there is not enough space in buffer
 * Output: the position in the buffer that it is safe to write to
 **/
UINT8 *getFileBuf(UINT32 size, UINT8 *fileBuf, UINT8 *curFilePos, FILE *file) {
    UINT16 bufSize = curFilePos - fileBuf;
    if((bufSize + size) > BUF_SIZE) {
        fwrite(fileBuf, UINT8_SIZE, bufSize, file);
        curFilePos = fileBuf;
    }

    return curFilePos;
}

/**
 * Function: startTrace
 * Description: Function used to mark when to start tracing.
 * Output: None
 **/
void startTrace() {
    printTrace = true;
}

/**
 * Function: printDataLabel
 * Description: Writes a data label with the given eventId to the buffer specified by pos
 * Assumptions: There is enough space in the buffer
 * Output: New current position in buffer
 **/
UINT8 *printDataLabel(UINT8 *pos, UINT64 eventId) {
    *pos = (UINT8) OP_LABEL;
    pos += UINT8_SIZE;

    *((UINT64 *) pos) = eventId;
    pos += UINT64_SIZE;

    return pos;
}

/**
 * Function: printMemData
 * Description: Writes a MemData entry into the location specified by pos given the memory's 
 *  address, value and size. Additionally, if sizePos is provided, it set the sizePos's value to
 *  the location of size in the buffer.
 * Assumptions: There is enough space in the buffer
 * Output: New current position in buffer
 **/
UINT8 *printMemData(UINT8 *pos, UINT16 size, ADDRINT addr, UINT8 *val, UINT8 **sizePos) {
    *pos = (UINT8) OP_MEM;
    pos += UINT8_SIZE;

    if(sizePos != NULL) {
        *sizePos = pos;
    }

    *((UINT16 *) pos) = size;
    pos += UINT16_SIZE;

    *((ADDRINT *) pos) = addr;
    pos += ADDRINT_SIZE;

    if(size != 0 && val != NULL) {
        memcpy(pos, val, size);
        pos += size;
    }

    return pos;
}

/**
 * Function: printExceptionEvent
 * Description: Writes an exception event into the location specified by pos
 * Assumptions: There is enough space in the buffer
 * Output: New current position in buffer
 **/
UINT8 *printExceptionEvent(UINT8 *pos, ExceptionType eType, INT32 info, THREADID tid, ADDRINT addr) {
    UINT8 *buf = pos;

    *pos = EVENT_EXCEPT; //event_type
    pos += UINT8_SIZE;

    *pos = eType; //type
    pos += UINT8_SIZE;

    *((INT32 *) pos) = info;
    pos += sizeof(INT32);

    *((UINT32 *) pos) = tid;
    pos += UINT32_SIZE;

    *((ADDRINT *) pos) = addr;
    pos += ADDRINT_SIZE;

    pos += UINT16_SIZE;

    *((UINT16 *)(pos - UINT16_SIZE)) = (UINT16)(pos - buf);

    return pos;
}

/**
 * Function: printDataReg
 * Description: Writes a data register entry for lReg into the buffer location specified by pos.
 *  It also fills in valBuf with the value of the register
 * Assumptions: There is enough space in the buffer, valBuf is at least 64 bytes
 * Side Effects: Fills in valBuf with register value
 * Output: New current position in buffer
 **/
UINT8 *printDataReg(THREADID tid, UINT8 *pos, LynxReg lReg, const CONTEXT *ctxt, UINT8 *valBuf) {
    REG reg = LynxReg2Reg(lReg);

    PIN_GetContextRegval(ctxt, reg, valBuf);

    *pos = (UINT8) OP_REG;
    pos += UINT8_SIZE;

    *((UINT32 *) pos) = tid;
    pos += UINT32_SIZE;

    *pos = (UINT8) lReg;
    pos += UINT8_SIZE;

    UINT32 size = lynxRegSize(lReg);
    memcpy(pos, valBuf, size);
    pos += size;

    return pos;
} 

/**
 * Function: checkInitializedStatus
 * Description: Checks to see if we have printed out the initial status of a thread.
 * Output: True if initialized, false otherwise
 **/
bool checkInitializedStatus(THREADID tid) {
    return !tls[tid].initialized;
}

/**
 * Function: initThread
 * Description: Records the initial state of a thread in the trace
 * Output: None
 **/
void initThread(THREADID tid, const CONTEXT *ctx) {
    if(!tls[tid].initialized) {
        PIN_MutexLock(&dataLock);
        recordRegState(tid, ctx);
        PIN_MutexUnlock(&dataLock);
        tls[tid].initialized = true;
    }
}

/**
 * Function: initIns
 * Description: Initializes the thread local storage for a new instruction. 
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL initIns(THREADID tid) {
    ThreadData &data = tls[tid];
    data.pos = data.buffer + UINT8_SIZE;
    data.dataPos = data.dataBuffer;

    //save instruction info in TLS
    *((UINT8 *) data.pos) = EVENT_INS;
    data.predRecord = data.pos;
    data.pos += UINT8_SIZE;

    //be predict everythign is right
    *data.predRecord |= 0xf8;

    //mark that this instruction needs to be printed
    data.print = true;
}

/**
 * Function: recordSrcId
 * Description: Records the source ID of the instruction in the thread local storage's trace buffer. 
 *  After the instruction executes, this buffer will be written to the trace.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordSrcId(THREADID tid, UINT32 srcId) {
    ThreadData &data = tls[tid];
    bool incorrect = (srcId != data.predSrcId);
    data.predSrcId = *((UINT32 *) data.pos) = srcId;
    *data.predRecord &= ~((incorrect) << PRED_SRCID);
    data.pos += (incorrect) ? UINT32_SIZE : 0;
}

/**
 * Function: contextChange
 * Description: Records information in a trace if an exception occurred, resulting in a context change. 
 *  Note, this is the only place that event ids are adjusted due to an exception event.
 * Output: None
 **/
void contextChange(THREADID tid, CONTEXT_CHANGE_REASON reason, const CONTEXT *fromCtx, CONTEXT *toCtx, INT32 info, void *v) {
    if(printTrace) {
        //check to see if an exception occurred. If so, print out info about it
        if(reason == CONTEXT_CHANGE_REASON_FATALSIGNAL || reason == CONTEXT_CHANGE_REASON_SIGNAL) {
            PIN_MutexLock(&traceLock);
            traceBufPos = getFileBuf(32, traceBuf, traceBufPos, traceFile);
            *traceBufPos = numSkipped;
            traceBufPos += 1;
            traceBufPos = printExceptionEvent(traceBufPos, LINUX_SIGNAL, info, tid, PIN_GetContextReg(fromCtx, REG_INST_PTR));
            eventId++;
            PIN_MutexUnlock(&traceLock);
        }
        else if(reason == CONTEXT_CHANGE_REASON_EXCEPTION) {
            PIN_MutexLock(&traceLock);
            traceBufPos = getFileBuf(32, traceBuf, traceBufPos, traceFile);
            *traceBufPos = numSkipped;
            traceBufPos += 1;
            traceBufPos = printExceptionEvent(traceBufPos, WINDOWS_EXCEPTION, info, tid, PIN_GetContextReg(fromCtx, REG_INST_PTR));
            eventId++;
            PIN_MutexUnlock(&traceLock);
        }
        /*Causing issues now because destRegs and destFlags are available, and apparently are not in the context
         else {
         printIns(tid, fromCtx);
         }*/
    }
}

/**
 * Function: recordRegState
 * Description: Records the register state for the current architecture in data file.
 * Output: None
 **/
void recordRegState(THREADID tid, const CONTEXT *ctxt) {
    UINT8 val[LARGEST_REG_SIZE];
    UINT8 *pos = getFileBuf(4096, dataBuf, dataBufPos, dataFile);

#if defined(TARGET_MIC) || defined(TARGET_IA32E)
    for(UINT32 lReg = LYNX_GR64_FIRST; lReg <= LYNX_GR64_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_IA32)
    for(UINT32 lReg = LYNX_X86_GR32_FIRST; lReg <= LYNX_X86_GR32_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_IA32) || defined(TARGET_IA32E)
    for(UINT32 lReg = LYNX_YMM_X86_FIRST; lReg <= LYNX_YMM_X86_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_IA32E)
    for(UINT32 lReg = LYNX_YMM_X64_FIRST; lReg <= LYNX_YMM_X64_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_MIC)
    for(UINT32 lReg = LYNX_ZMM_FIRST; lReg <= LYNX_ZMM_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_K_MASK_FIRST; lReg <= LYNX_K_MASK_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif

    for(UINT32 lReg = LYNX_SEG_FIRST; lReg <= LYNX_SEG_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_SSE_FLG_FIRST; lReg <= LYNX_SSE_FLG_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_FPU_FIRST; lReg <= LYNX_FPU_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_FPU_STAT_FIRST; lReg <= LYNX_FPU_STAT_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }

    //for some reason if I try to print these out, PIN will crash
#if 0
    for(UINT32 lReg = LYNX_DBG_FIRST; lReg <= LYNX_DBG_LAST; lReg++) {
        printReg(tid, (LynxReg)lReg, ctxt, val, file);
    }
    for(UINT32 lReg = LYNX_CTRL_FIRST; lReg <= LYNX_CTRL_LAST; lReg++) {
        printReg(tid, (LynxReg)lReg, ctxt, val, file);
    }
#endif

    dataBufPos = pos;

    //fwrite(buf, UINT8_SIZE, pos - buf, dataFile);

}

/**
 * Function: threadStart
 * Description: Call this when another thread starts so we can accurately track the number of threads 
 *  the program has. We also need to initialize the thread local storage for the new thread. Note, we 
 *  check here to make sure we are still within maxThreads
 * Output: None
 **/
VOID threadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, void *v) {
    if(tid >= maxThreads) {
        fprintf(stderr, "More than %u threads are being used", maxThreads);
        exit(1);
    }
    //we've got another thread
    numThreads++;

    ThreadData &data = tls[tid];
    data.initialized = false;
    data.pos = data.buffer;
    data.dataPos = data.dataBuffer;
    data.predRecord = NULL;
    data.print = false;
    data.destRegs = NULL;
    data.destFlags = 0;
    data.memWriteSize = 0;
}

/**
 * Function: setupFile
 * Description: Opens and sets up any output files. Note, since this is an ASCII trace, it will setup 
 * trace.out according to the Data Ops trace file format.
 * Output: None
 **/
void setupFile(UINT16 infoSelect) {
    traceFile = fopen("trace.out", "wb");
    dataFile = fopen("data.out", "w+b");
    errorFile = fopen("errors.out", "w");

    FileHeader h;
    h.ident[0] = 'U';
    h.ident[1] = 'A';
    h.ident[2] = 'T';
    h.ident[3] = 'R';
    h.ident[4] = 'C';
    h.ident[5] = 2;
    h.ident[6] = 0;
    h.ident[7] = 0;
    h.ident[8] = 0;

    h.traceType = DATA_OPS_TRACE;

#if defined(TARGET_IA32)
    h.archType = X86_ARCH;
#else
    h.archType = X86_64_ARCH;
#endif

    int x = 1;
    char *c = (char *) &x;

    //we only support little-endian right now.
    assert(c);

    h.machineFlags = *c;
    h.sectionNumEntry = 5;

    sectionOff = sizeof(FileHeader);
    UINT32 sectionSize = sizeof(SectionEntry);
    traceHeaderOff = sectionOff + h.sectionNumEntry * sectionSize;
    UINT64 infoSelHeaderOff = traceHeaderOff + sizeof(TraceHeader);
    traceStart = infoSelHeaderOff + sizeof(InfoSelHeader);

    h.sectionTableOff = sectionOff;

    h.sectionEntrySize = sectionSize;


    fwrite(&h, sizeof(FileHeader), 1, traceFile);

    SectionEntry entries[h.sectionNumEntry];

    strncpy((char *) entries[0].name, "TRACE", 15);
    entries[0].offset = traceStart;
    entries[0].type = TRACE_SECTION;

    strncpy((char *) entries[1].name, "DATA", 15);
    entries[1].type = DATA_SECTION;

    strncpy((char *) entries[2].name, "TRACE_HEADER", 15);
    entries[2].type = TRACE_HEADER_SECTION;
    entries[2].offset = traceHeaderOff;
    entries[2].size = sizeof(TraceHeader);

    strncpy((char *) entries[3].name, "STR_TABLE", 15);
    entries[3].type = STR_TABLE;

    /*strncpy((char *) entries[4].name, "SEGMENT_LOADS", 15);
    entries[4].type = SEGMENT_LOADS;*/

    strncpy((char *) entries[4].name, "INFO_SEL_HEAD", 15);
    entries[4].type = INFO_SEL_HEADER;
    entries[4].offset = infoSelHeaderOff;
    entries[4].size = sizeof(InfoSelHeader);

    //entries[5].offest = 

    fwrite(entries, sectionSize, h.sectionNumEntry, traceFile);

    TraceHeader traceHeader;
    fwrite(&traceHeader, sizeof(TraceHeader), 1, traceFile);

    InfoSelHeader infoSelHeader;
    infoSelHeader.selections = infoSelect;
    fwrite(&infoSelHeader, sizeof(InfoSelHeader), 1, traceFile);
}

// ===========================================================

/**
 * Function: write2Json
 * Description: This function writes modeled IR to a JSON file in formatted structure.
 * Input: None.
 * Output: None.
 **/
void write2Json() {
    ofstream jsonFile;
    jsonFile.open("ir.json");

    int counter = 0;

    jsonFile << "{" << endl;
    jsonFile << "   \"is_ir\": true," << endl;
    jsonFile << "   \"nodes\": [" << endl;
    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        Node *node = IRGraph->nodes[i];
        jsonFile << "       {" << endl;
        jsonFile << "           \"id\": " << dec << node->id << "," << endl; 
        jsonFile << "           \"alive\": ";
        if (node->alive) {
            jsonFile << "true," << endl;
        }
        else {
            jsonFile << "false," << endl;
        }
        jsonFile << "           \"address\": \"" << hex << node->intAddress << "\"," << endl; 
        jsonFile << "           \"opcode\": \"" << hex << node->opcode << "\"," << endl; 
        jsonFile << "           \"is_nonIR\": ";
        if (node->is_nonIR) {
            jsonFile << "true," << endl;
        }
        else {
            jsonFile << "false," << endl;
        }
        jsonFile << "           \"size\": " << dec << node->size << "," << endl; 
        // Write edge information.
        jsonFile << "           \"edges\": [";
        for (int i = 0; i < node->numberOfEdges; i++) {
            if (node->edgeNodes[i] != NULL) {
                jsonFile << dec << node->edgeNodes[i]->id;
            }
            else {
                jsonFile << dec << INT_INVALID;
            }

            if (i < node->numberOfEdges-1) {
                jsonFile << ",";
            }
        }
        jsonFile << "]," << endl;
        // Write occupied memory location and the value informations
        jsonFile << "           \"directValues\": {" << endl;
        for (int i = 0; i < node->numberOfLocs; i++) {
            jsonFile << "               \"" << dec << node->offsets[i] << "\": ";
            jsonFile << hex << "\"" << node->valuesInLocs[i] << "\"";

            if (i < node->numberOfLocs-1) {
                jsonFile << "," << endl;
            }
            else {
                jsonFile << endl;
            }
        }
        jsonFile << "           }," << endl;
        // Write opcode optimization information.
        counter = 0;
        jsonFile << "           \"opcode_update\": {" << endl;
        map<int,ADDRINT>::iterator itOpUpdate;
        for (itOpUpdate = node->id2Opcode.begin(); itOpUpdate != node->id2Opcode.end();) {
            jsonFile << "               \"" << dec << itOpUpdate->first << "\":";
            jsonFile << "\"" << hex << itOpUpdate->second << "\"";
            if (++itOpUpdate != node->id2Opcode.end()) {
                jsonFile << "," << endl;
            }
            else {
                jsonFile << endl;
            }
        }
        jsonFile << "           }," << endl;
        // Write added optimization information.
        jsonFile << "           \"added\": {" << endl;
        map<int,int>::iterator itAdd;
        for (itAdd = node->instOrder2addNodeId.begin(); itAdd != node->instOrder2addNodeId.end();) {
            jsonFile << "               \"" << dec << itAdd->first << "\":" << itAdd->second;
            if (++itAdd != node->instOrder2addNodeId.end()) {
                jsonFile << "," << endl;
            }
            else {
                jsonFile << endl;
            }
        }
        jsonFile << "           }," << endl;
        // Write removed optimization information.
        jsonFile << "           \"removed\": {" << endl;
        map<int,int>::iterator itRem;
        for (itRem = node->instOrder2remNodeId.begin(); itRem != node->instOrder2remNodeId.end();) {
            jsonFile << "               \"" << dec << itRem->first << "\":" << itRem->second;
            if (++itRem != node->instOrder2remNodeId.end()) {
                jsonFile << "," << endl;
            }
            else {
                jsonFile << endl;
            }
        }
        jsonFile << "           }," << endl;
        // Write replaced optimization information.
        jsonFile << "           \"replaced\": {" << endl;
        map<int,ReplacedInfo>::iterator itRep;
        for (itRep = node->instOrder2repInfo.begin(); itRep != node->instOrder2repInfo.end();) {
            jsonFile << "               \"" << dec << itRep->first << "\": {" << endl;
            jsonFile << "                   \"from\":" << (itRep->second).nodeIdFrom << "," << endl;
            jsonFile << "                   \"to\":" << (itRep->second).nodeIdTo << endl;

            if (++itRep != node->instOrder2repInfo.end()) {
                jsonFile << "                }," << endl;
            }
            else {
                jsonFile << "                }" << endl;
            }
        }
        jsonFile << "           }," << endl;
        // Write direct value optimization information.
        jsonFile << "           \"directValueOpt\": {" << endl;
        map<int, DirectValOpt>::iterator itDir;
        for (itDir = node->instOrder2dirValOpt.begin(); itDir != node->instOrder2dirValOpt.end();) {
            jsonFile << "               \"" << dec << itDir->first << "\": {" << endl;
            jsonFile << "                   \"offset\":" << dec << (itDir->second).offset << "," << endl;
            jsonFile << "                   \"valFrom\":\"" << hex << (itDir->second).valFrom << "\"," << endl;
            jsonFile << "                   \"valTo\":\"" << hex << (itDir->second).valTo << "\"," << endl;
            if ((itDir->second).is_update ) {
                jsonFile << "                   \"is_update\": true" << endl;
            }
            else {
                jsonFile << "                   \"is_update\": false" << endl;
            }

            if (++itDir != node->instOrder2dirValOpt.end()) {
                jsonFile << "               }," << endl;
            }
            else {
                jsonFile << "               }" << endl;
            }
        }
        jsonFile << "           }," << endl;
        // Node access (evaluate) information.
        jsonFile << "           \"evaluates\": {" << endl;
        map<int, Offset2Value>::iterator itOff;
        for (itOff = node->instOrder2offVal.begin(); itOff != node->instOrder2offVal.end();) {
            jsonFile << "               \"" << dec << itOff->first << "\": {" << endl;
            jsonFile << "                   \"offset\":" << dec << (itOff->second).offset << "," << endl;
            jsonFile << "                   \"value\":\"" << hex << (itOff->second).value << "\"" << endl;

            if (++itOff != node->instOrder2offVal.end()) {
                jsonFile << "               }," << endl;
            }
            else {
                jsonFile << "               }" << endl;
            }
        }
        jsonFile << "           }," << endl;
        // Instruction access log information.
        jsonFile << "           \"instAccess\": {" << endl;
        map<int, InstInfo>::iterator itinstInfo;
        for (itinstInfo = node->instInfo.begin(); itinstInfo != node->instInfo.end();) {
            jsonFile << "               \"" << dec << itinstInfo->first << "\": {" << endl;
            jsonFile << "                   \"address\":" << dec << (itinstInfo->second).address << "," << endl;
            jsonFile << "                   \"fnCallRetId\":" << dec << (itinstInfo->second).fnCallRetId;
            jsonFile << "," << endl;
            jsonFile << "                   \"fnId\":" << dec << (itinstInfo->second).fnId << "," << endl;
            string binString = uint8Tostring((itinstInfo->second).binary, (itinstInfo->second).instSize);
            jsonFile << "                   \"binary\":\"" << binString << "\"," << endl;
            jsonFile << "                   \"type\":" << dec << (itinstInfo->second).accessType << endl;

            if (++itinstInfo != node->instInfo.end()) {
                jsonFile << "               }," << endl;
            }
            else {
                jsonFile << "               }" << endl;
            }
        }
        jsonFile << "           }" << endl;
        // Closing
        if (i < IRGraph->lastNodeId-1) {
            jsonFile << "       }," << endl;
        }
        else {
            jsonFile << "       }" << endl;
        }
    }
    jsonFile << "   ]," << endl;
    // Print function information.
    map<UINT32, string> fnId2fnStr;
    map<int, UINT32>::iterator itfnCallRetId2fnId1;
    itfnCallRetId2fnId1 = fnCallRet.begin();
    while (itfnCallRetId2fnId1 != fnCallRet.end()) {
        string fn = strTable.get(itfnCallRetId2fnId1->second);
        fnId2fnStr[itfnCallRetId2fnId1->second] = fn;
        ++itfnCallRetId2fnId1;
    }
    jsonFile << "   \"fnId2Name\": {" << endl;
    map<UINT32, string>::iterator itfnId2fnStr;
    for (itfnId2fnStr = fnId2fnStr.begin(); itfnId2fnStr != fnId2fnStr.end();) {
        jsonFile << "       \"" << dec << itfnId2fnStr->first << "\":";
        jsonFile << "\"" << itfnId2fnStr->second << "\"";

        if (++itfnId2fnStr != fnId2fnStr.end()) {
            jsonFile << "," << endl;
        }
        else {
            jsonFile << endl;
        }
    }
    jsonFile << "   }," << endl;
    // Print fnCallRetId-fnId information.
    counter = 0;
    jsonFile << "   \"fnCallRetId2fnId\": {" << endl;
    map<int, UINT32>::iterator itfnCallRetId2fnId2;
    for (itfnCallRetId2fnId2 = fnCallRet.begin(); itfnCallRetId2fnId2 != fnCallRet.end();) {
        jsonFile << "       \"" << dec << itfnCallRetId2fnId2->first << "\":";
        jsonFile << itfnCallRetId2fnId2->second;

        if (++itfnCallRetId2fnId2 != fnCallRet.end()) {
            jsonFile << "," << endl;
        }
        else {
            jsonFile << endl;
        }
        counter++;
    }
    jsonFile << "   }," << endl;
    // Print all memory writes happened during the JIT compilation.
    jsonFile << "   \"memory_writes\": {" << endl;
    map<ADDRINT, MWInst>::iterator itWrites;
    for (itWrites = writes.begin(); itWrites != writes.end();) {
        jsonFile << "       \"" << hex << itWrites->first << "\":";
        jsonFile << "\"" << hex << (itWrites->second).value << "\"";
        if (++itWrites != writes.end()) {
            jsonFile << "," << endl;
        }
        else {
            jsonFile << endl;
        }
    }
    jsonFile << "   }," << endl;
    // Print all memory reads happened during the JIT compilation.
    jsonFile << "   \"memory_reads\": {" << endl;
    map<ADDRINT, MRInst>::iterator itReads;
    for (itReads = reads.begin(); itReads != reads.end();) {
        jsonFile << "       \"" << hex << itReads->first << "\":";
        jsonFile << "\"" << hex << (itReads->second).value << "\"";
        if (++itReads != reads.end()) {
            jsonFile << "," << endl;
        }
        else {
            jsonFile << endl;
        }
    }
    jsonFile << "   }" << endl;
    jsonFile << "}" << endl;
}

/**
 * Function: endFile
 * Description: This function is being called at the end of tracing.
 *  Currently, this function simply calls write2Json to write the modeled IR to a JSON file.
 * Input: None.
 * Output: None.
 **/
void endFile() {
    // Write IR to a file in JSON.
    write2Json();

    //map<int,UINT32>::iterator it;
    //cout << "Size: " << fnCallRet.size() << endl;
    //for (it = fnCallRet.begin(); it != fnCallRet.end(); ++it) {
    //    cout << dec << "(" << it->first << ", " << it->second << ")" << endl;
    //}
}
