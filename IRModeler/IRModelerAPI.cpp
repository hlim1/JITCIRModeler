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
using std::endl;
using std::map;
using std::string;
using std::hex;
using std::dec;

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

// Thread local storage structure (for those unfamiliar with C++ structs, they are essentially classes 
// with different privacy rules so you can have functions)
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
    int id;                   // instruction id.
} instruction;

// Struct for source register information.
struct SrcReg {
    int instId;                // instruction id.
    UINT32 fnId;               // function id.
    UINT32 instOp;             // instruction opcode.
    LynxReg lReg;              // lynx register.
    ADDRINT value;             // value in the source register.
};

// Struct for memory write information.
struct MWInst {
    UINT32  fnId;              // function id.
    ADDRINT location;          // address location.
    ADDRINT value;             // written value.
    ADDRINT valueSize;         // size of written value in bytes.
    SrcReg  srcRegs[MAX_REGS]; // values in the read registers.
    int regSize = 0;           // number of register values collected.
};

// Struct for memory read information.
struct MRInst {
    UINT32  fnId;              // function id.
    ADDRINT location;          // address location.
    ADDRINT value;             // read value.
    ADDRINT valueSize;         // size of read value in bytes.
    SrcReg  srcRegs[MAX_REGS]; // values in the read registers.
    int regSize = 0;           // number of register values collected.
};

// Structures for holding memory/register reads and writes.
map<ADDRINT,MWInst> writes;     // Write location address to MWInst object.
map<ADDRINT,MRInst> reads;      // Read location address to MRInst object.
map<ADDRINT,MWInst> targetMWs;  // Write location address to MWInst object (Target Insts. Only).
map<int,SrcReg> targetSrcRegs;  // targetSrcRegsKey to SrcReg object.

SrcReg srcRegsHolder[MAX_REGS];
int regSize = 0;
UINT8  currentRaxVal[LARGEST_REG_SIZE];
UINT32 currentRaxValSize = 0;
bool populate_regs = false;
ADDRINT lastMemReadLoc = ADDRINT_INVALID;
int targetSrcRegsKey = 0;

bool is_former_range = false;

void constructModeledIRNode(UINT32 fnId, UINT32 system_id) {

    // Create a new node object and populate it with data.
    Node *node = new Node();
    node->id = IRGraph->lastNodeId;
    
    // Get node address.
    node->intAddress = get_node_address(fnId, system_id);
    assert (node->intAddress != ADDRINT_INVALID);

    // Get block head address.
    node->blockHead = get_node_block_head(node->intAddress, system_id);
    assert (node->blockHead != ADDRINT_INVALID);

    // Get node size.
    node->size = get_size(node->blockHead, system_id);
    assert (node->size != ADDRINT_INVALID);

    // Get block tail address.
    node->blockTail = node->blockHead + node->size;
    assert (node->blockTail != ADDRINT_INVALID);

    // Get opcode of a node.
    ADDRINT *opcode;
    opcode = get_opcode(node, system_id);
    node->opcode = opcode[0];
    node->opcodeAddress = opcode[1];
    assert (node->opcode != ADDRINT_INVALID);
    assert(node->opcodeAddress != ADDRINT_INVALID);

    // DEBUG
    std::cout << "Node Address: " << std::hex << node->intAddress << "; ";
    std::cout << "Opcode: " << std::hex << node->opcode << "(" << node->opcodeAddress << ")";
    std::cout << std::endl;

    // Get initial (in)direct value assigned to the node block.
    get_init_block_locs(node, system_id);

    // Add node to IRGraph.
    assert(MAX_NODES > IRGraph->lastNodeId);
    IRGraph->nodes[IRGraph->lastNodeId] = node;
    IRGraph->nodeAddrs[IRGraph->lastNodeId] = node->intAddress;
    IRGraph->lastNodeId += 1;

    // Reset temporary value holders.
    currentRaxValSize = 0;
    targetMWs.clear();
    targetSrcRegs.clear();
    is_former_range = false;
}

ADDRINT get_node_address(UINT32 fnId, UINT32 system_id) {

    ADDRINT address = ADDRINT_INVALID;

    if (system_id == V8) { 
        address = get_address_v8();
    }
    else if (system_id == JSC) {
        address = get_address_jsc();
    }

    assert(address != ADDRINT_INVALID);

    return address;
}

ADDRINT get_address_v8() {

    return uint8Toaddrint(currentRaxVal, currentRaxValSize);
}

ADDRINT get_address_jsc() {

    return uint8Toaddrint(currentRaxVal, currentRaxValSize);
}

ADDRINT *get_opcode(Node *node, UINT32 system_id) {

    static ADDRINT *opcode;

    if (system_id == V8) {
        opcode = get_opcode_v8(node);    
    }
    else if (system_id == JSC) {
        opcode = get_opcode_jsc(node);    
    }

    return opcode;
}

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
                if (
                        node->intAddress == read.srcRegs[i].value)
                {
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

ADDRINT get_node_block_head(ADDRINT address, UINT32 system_id) {

    ADDRINT blockHead = ADDRINT_INVALID;

    if (system_id == V8) {
        blockHead = get_node_block_head_v8(address);
    }
    else if (system_id == JSC) {
        // For JSC, block head address is equal to the node address.
        blockHead = address;
    }

    return blockHead;
}

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
                // set initial node edges.
                node->edgeNodes[node->numberOfEdges] = IRGraph->nodes[edgeNodeId];
                node->edgeAddrs[node->numberOfEdges] = write.location;
                node->numberOfEdges++;
            }
            else if (edgeNodeId == INT_INVALID && write.location != node->intAddress) {
                bool is_direct = isDirectAssignment(write.value);
                if (is_direct) {
                    // Compute the distance between the block head and the written location,
                    // then write to node's offsets to track which locations are wrriten.
                    ADDRINT offset = write.location - blockHead;
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

bool isDirectAssignment(ADDRINT value) {

    bool isDirectAssignment = true;

    map<ADDRINT,MWInst>::iterator it;
    it = writes.find(value);
    if (it != writes.end()) {
        isDirectAssignment = false;
    }

    return isDirectAssignment;
}

bool elemInMap(ADDRINT elem, map<ADDRINT,ADDRINT> targetMap) {
    bool is_exists = false;

    if (targetMap.find(elem) != targetMap.end()) {
        is_exists = true;
    }

    return is_exists;
}

bool checkRAXValue(UINT8 *value) {

    bool is_valid = true;

    if (value[2] == 0x00) {
        is_valid = false;
    }

    return is_valid;
}

ADDRINT uint8Toaddrint(UINT8* target, UINT32 size) {
    ADDRINT to = 0;
    for (int i = size-1; i >= 0; i--) {
        to = (to << 8) | target[i];
    }
    
    return to;
}

UINT8* addrintTouint8(ADDRINT target, UINT32 size) {
    UINT8 *to = new UINT8();
    memcpy(to, &target, size);

    return to;
}

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

bool fnInAllocs(string fn) {
    bool is_exists = false;
    for (int i = 0; i < NODE_ALLOC_SIZE; i++) {
        if (fn == NODE_BLOCK_ALLOCATORS[i]) {
            is_exists = true;
            break;
        }
    }

    return is_exists;
}

bool fnInFormers(string fn) {
    bool is_exists = false;
    for (int i = 0; i < NODE_FORMERS_SIZE; i++) {
        if (fn == NODE_FORMERS[i]) {
            is_exists = true;
            break;
        }
    }

    return is_exists;
}

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

ADDRINT get_size(ADDRINT address, UINT32 system_id) {

    ADDRINT size = ADDRINT_INVALID;

    if (system_id == V8) {
        size = get_size_v8(address);
    }
    else if (system_id == JSC) {
        size = get_size_jsc(address);
    }

    return size;
}

ADDRINT get_size_v8(ADDRINT address) {

    ADDRINT size = ADDRINT_INVALID;
    SrcReg target;
    bool got_target = false;

    // First, identify the register that holds block memory address.
    map<int,SrcReg>::iterator it;
    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        SrcReg reg = it->second;
        if (reg.instOp == ADD and reg.value == address) {
            target = reg;
            got_target = true;
            break;
        }
    }
    assert (got_target);

    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        SrcReg reg = it->second;
        if (reg.instId == target.instId && reg.value != target.value) {
            size = reg.value;
            break;
        }
    }
    assert (size != ADDRINT_INVALID);

    return size;
}

ADDRINT get_size_jsc(ADDRINT address) {

    ADDRINT size = ADDRINT_INVALID;
    SrcReg target;
    bool got_target = false;

    // First, identify the register that holds block memory address.
    map<int,SrcReg>::iterator it;
    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        SrcReg reg = it->second;
        if (reg.instOp == ADD and reg.value == address) {
            target = reg;
            got_target = true;
            break;
        }
    }
    assert (got_target);

    for (it = targetSrcRegs.begin(); it != targetSrcRegs.end(); ++it) {
        SrcReg reg = it->second;
        if (reg.instId == target.instId && reg.value != target.value) {
            size = reg.value;
            break;
        }
    }
    assert (size != ADDRINT_INVALID);

    return size;
}

/**
 *  Function: checkCopiedValue
 *  Description: Check a value copied by PIN_SafeCopy. If the copied value has
 *  an incorrect format, e.g., 0040032c3bef7f00, where the first byte of '00' is
 *  moved to the end, return false. Otherwise, true.
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
 * on the input value, e.g., fix 0040032c3bef7f00 to 40032c3bef7f0000.
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

ADDRINT getUpdatedOpcode(ADDRINT value, ADDRINT valueSize) {

    ADDRINT opcode = ADDRINT_INVALID;

    if (valueSize == V8_OPCODE_SIZE || valueSize == JSC_OPCODE_SIZE) {
        opcode = value;
    }

    return opcode;
}

// Functions for prints for debugging.

void printUINT8(UINT8* arr, UINT32 size) {

    for (UINT32 i = size-1; i > 0; i--) {
        printf("%02x", arr[i]);
    }
    printf("%02x\n", arr[0]);
}

void printNodes() {

    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        printNode(IRGraph->nodes[i]);
    }
}

void printNode(Node *node) {

    cout << "ID: " << dec << node->id << "; ";
    cout << "Liveness: " << node->alive << "; ";
    cout << "Address: " << hex << node->intAddress << " (Block address: ";
    cout << node->blockHead << ", " << node->blockTail << "); ";
    cout << "Opcode: " << node->opcode << "; ";
    cout << "Size: " << hex << node->size << endl;
    cout << "Edges (Edge addr. -> Node ID):" << endl;
    for (int i = 0; i < node->numberOfEdges; i++) {
        cout << hex << node->edgeAddrs[i] << " -> ";
        if (node->edgeNodes[i] != NULL) {
            cout << dec << node->edgeNodes[i]->id << endl;
        }
        else {
            cout << dec << INT_INVALID << endl;
        }
    }
    cout << "Edge Add Optimization (fnOrderId: added node id):" << endl;
    map<int, int>::iterator it1;
    for (it1 = node->fnOrder2addNodeId.begin(); it1 != node->fnOrder2addNodeId.end(); ++it1) {
        cout << dec << it1->first << ": " << it1->second << endl;
    }
    cout << "Written values (Distance from block head -> value):" << endl;
    for (int i = 0; i < node->numberOfLocs; i++) {
        cout << "+" << dec << node->offsets[i] << " -> ";
        if (node->valuesInLocs[i] != ADDRINT_INVALID) {
            cout << hex << node->valuesInLocs[i] << endl;
        }
        else {
            cout << hex << ADDRINT_INVALID << endl;
        }
    }
    cout << "Direct values optimization information (fnOrderId: (offset: value from -> value to)):" << endl;
    map<int, DirectValOpt>::iterator it;
    for (it = node->fnOrder2dirValOpt.begin(); it != node->fnOrder2dirValOpt.end(); ++it) {
        cout << dec << it->first << ": (" << (it->second).offset << ": ";
        cout << hex << (it->second).valFrom << " -> " << (it->second).valTo << ")" << endl; 
    }
    cout << "Function access information (accessOrder: fnId, accessType)" << endl;
    map<int, FnInfo>::iterator it2;
    for (it2 = node->fnInfo.begin(); it2 != node->fnInfo.end(); ++it2) {
        cout << dec << it2->first << ": " << (it2->second).fnId << ", " << (it2->second).accessType << endl;
    }
    cout << "--" << endl;
}

void printMap(map<ADDRINT,ADDRINT> mymap) {
    for (map<ADDRINT,ADDRINT>::iterator it = mymap.begin(); it != mymap.end(); ++it) {
        cout << hex << it->first << ", " << it->second << endl;
    }
}

// =============================================================

/**
 * Function: recordSrcRegs
 * Description: Records information on source registers of an instruction.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordSrcRegs(
        THREADID tid, const CONTEXT *ctx, const RegVector *srcRegs, UINT32 fnId,
        UINT32 instOpcode) {

    // In case regSize was not reset, reset it to zero.
    regSize = 0;

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
        SrcReg srcReg;
        srcReg.instId = instruction.id;
        srcReg.fnId = fnId;
        srcReg.instOp = instOpcode;
        srcReg.lReg = fullLReg;
        srcReg.value = regValueInt;

        assert (i < MAX_REGS);
        srcRegsHolder[regSize] = srcReg;
        regSize++;
    }
}

/**
 * Function: recordDestRegs
 * Description: Records the instruction's destination registers and flags in thread local storage so
 * that they can be printed after the instruction executes.
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
 **/
void checkMemRead(ADDRINT readAddr, UINT32 readSize, UINT32 fnId) {

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
    reads[readAddr] = read;
    // Track the last added key.
    lastMemReadLoc = readAddr;
    // Mark that the tool needs to update the register.
    populate_regs = true;

    // Check if the current memory location belongs to any one of existing node.
    ADDRINT nodeId = ADDRINT_INVALID;
    bool is_node_block = false;
    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        Node *node = IRGraph->nodes[i];
        // Check if the memory read access happens to some node block.
        if (
                (readAddr >= node->blockHead && readAddr < node->blockTail) ||
                (valueInt >= node->blockHead && valueInt < node->blockTail)
        ) {
            nodeId = node->id;
            is_node_block = true;
            break;
        }
    }
    // If the access was happened, update the node's access log.
    if (is_node_block) {
        Node *accessed =  IRGraph->nodes[nodeId];
        updateLogInfo(accessed, fnId, EVALUATE);
    }

    PIN_MutexUnlock(&dataLock);
}

void check2MemRead(ADDRINT readAddr1, ADDRINT readAddr2, UINT32 readSize, UINT32 fnId) {
    checkMemRead(readAddr1, readSize, fnId);
    checkMemRead(readAddr2, readSize, fnId);
}

/**
 * Function: recordMemWrite
 * Description: We can only get the address of a memory write from PIN before an instruction executes.
 *  since we need this information after the instruction executes (so we can get the new values), we need
 *  to record the address and size of the memory write in thread local storage so we can get the information
 *  later.
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
 * Output: None
 **/
bool analyzeRecords(THREADID tid, const CONTEXT *ctx, UINT32 fnId, UINT32 opcode, bool is_create) {

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
        analyzeRegWrites(tid, ctx);
        data.destRegs = NULL;
    }

    // If the instruction has memory write, then analyze memory write, e.g., MW[..]=...
    if(data.memWriteSize != 0) {
        analyzeMemWrites(tid, fnId, is_former_range);
        data.memWriteSize = 0; 
    }

    // Keep tracks of source register information of the node allocator function instructions
    // separately.
    if (is_former_range || fnInAllocs(fn)) {
        for (int i = 0; i < regSize; i++) {
            targetSrcRegs[targetSrcRegsKey] = srcRegsHolder[i];
            targetSrcRegsKey++;
        } 
    }

    // If there was a memory read in the current node allocator function instruction, then populate
    // the source register information to the MR tracker object.
    if (populate_regs) {
        for (int i = 0; i < regSize; i++) {
            reads[lastMemReadLoc].srcRegs[i] = srcRegsHolder[i];
        }
        reads[lastMemReadLoc].regSize = regSize;
        populate_regs = false;
    }
    memset(srcRegsHolder, 0, regSize);
    regSize = 0;

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

void analyzeRegWrites(THREADID tid, const CONTEXT *ctx) {
    
    ThreadData &data = tls[tid];

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
                PIN_SafeCopy(currentRaxVal, buf, fullSize);
                currentRaxValSize = fullSize;
            }
        }
    }
}

void analyzeMemWrites(THREADID tid, UINT32 fnId, bool is_range) {

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
    for (int i = 0; i < regSize; i++) {
        write.srcRegs[i] = srcRegsHolder[i];
    }
    write.regSize = regSize;

    // Add the 'write' object to the 'writes' map.
    writes[data.memWriteAddr] = write;

    // Collect location and value in MWs only in the node former function instructions.
    if (is_range) {
        targetMWs[data.memWriteAddr] = write;
    }

    // If there are IR nodes, analyzes and extracts, if exists, optimization information.
    if (IRGraph->lastNodeId > 0) {
        trackOptimization(write.location, write.value, data.memWriteSize, fnId);
    }
}

void trackOptimization(ADDRINT location, ADDRINT value, ADDRINT valueSize, UINT32 fnId) {

    // Check if the current memory location belongs to any one of existing node.
    ADDRINT nodeId = ADDRINT_INVALID;
    bool is_node_addr = false;
    //bool is_opcode_update = false;
    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        Node *node = IRGraph->nodes[i];
        if (
                (location >= node->blockHead && location < node->blockTail) ||
                location == node->opcodeAddress
        ) {
            // If the writing location is within the range of one node, then get the node id.
            nodeId = node->id;
            // If the writing location is the address of a node, then mark 'is_node_addr' to true.
            if (location == node->intAddress) {
                is_node_addr = true;
            }
            // If the currently analyzing write is an opcode update, then mark is_opcode_update to true.
            //if (location == node->opcodeAddress) {
            //    is_opcode_update = true;
            //}
            break;
        }
    }

    // If the location belongs to one of the nodes,
    if (nodeId != ADDRINT_INVALID) {
        Node *node = IRGraph->nodes[nodeId];
        // Check if the memory write value is an opcode.
        //ADDRINT opcode = getUpdatedOpcode(value, valueSize);

        // Check if the memory write value is an address of a node.
        ADDRINT value_id = compareValuetoIRNodes(value);
        // Check if the location is already occupied edge.
        int edge_idx = getEdgeIdx(node, location);
        // If the location is an already occupied edge, handle edge removal or edge replace.
        if (edge_idx != INT_INVALID) {
            // If value is '0', which is to wipe out the memory location, handle edge 'removal'.
            if (value == WIPEMEM) {
                edgeRemoval(node, edge_idx, fnId);
            }
            // If value is one of the existing nodes, handle edge 'replace'.
            else if (value_id != ADDRINT_INVALID) {
                edgeReplace(node, value_id, edge_idx, fnId);
            }
        }
        // If the location is not occupied edge, but the value is a node, handle edge 'addition'.
        else if (edge_idx == INT_INVALID && value_id != ADDRINT_INVALID) {
            edgeAddition(node, location, value_id, fnId);
        }
        // If the location belongs to some node block, but it's not an edge and the value is not a node.
        else if (edge_idx == INT_INVALID && value_id == ADDRINT_INVALID) {
            // Node's are being destroyed and the location is being wiped by writing '0'.
            // We check such pattern in the instruction and mark the node dead.
            if (is_node_addr && value == WIPEMEM) {
                nodeDestroy(node, fnId);
            }
            // If the write is happening at non-node address location, then check for
            // the value assignment (direct & indirect).
            else if (!is_node_addr) {
                bool is_direct = isDirectAssignment(value);
                // TODO: 'valueSize < 8' is to prevent considering the memory address is considered as
                // a value with an assmption is that the address's size is 8. This is not a good approach
                // so we need to update it with more appropriate way.
                if (is_direct && valueSize < 8) {
                    directValueWrite(node, location, value, fnId); 
                }
                else {
                    // TODO: Need to handle indirect (pointing to non-ir object) assignment.
                }
            }
        }
    }
}

void updateLogInfo(Node *node, UINT32 fnId, Access accessType) {

    // Retrieve the function name from the table.
    string fnName = strTable.get(fnId);

    // Create a new FnInfo object and update it.
    FnInfo fnInfo;
    fnInfo.fnId = fnId;
    fnInfo.accessType = accessType;

    // Avoid adding duplicate fnInfo one after another.
    if ((node->fnInfo).empty() || !isSameAccess(node, fnInfo)) {
        // Update the node's function info. map.
        node->fnInfo[IRGraph->fnOrderId] = fnInfo;
        node->lastInfoId = IRGraph->fnOrderId;

        // Update IR's function order id and map.
        IRGraph->fnId2Name[fnId] = fnName;
        IRGraph->fnOrderId++;
    }
}

bool isSameAccess(Node *node, FnInfo fnInfo) {

    bool is_same = false;
    
    // Get the node's last fnInfo.
    FnInfo latest = node->fnInfo[node->lastInfoId];

    // If the currently analyzing fnInfo's fnId and accessType are
    // equal to the node's last fnInfo, set is_same to true.
    // Otherwise, false.
    if (
            latest.fnId == fnInfo.fnId
            and latest.accessType == fnInfo.accessType

    ) {
        is_same = true;
    }
    else {
        is_same = false;
    }

    return is_same;
}

void edgeRemoval(Node *node, int edge_idx, UINT32 fnId) {

    Node *target = node->edgeNodes[edge_idx];
    // In some cases, I see an instruction that cleans memory location where it's alreay
    // wiped out with '0' and never written with other value. Such instruction will clean
    // the node's edge (already to get NULL). Thus, the earlier line will return NULL.
    // We only count the edge removal only if the edge has a valid node, so we handle
    // this case by checking that the returned value from the earlier line is NOT NULL.
    if (target != NULL) {
        // Update node's 'remove' optimization information.
        node->fnOrder2remNodeId[IRGraph->fnOrderId] = target->id;
        // Set the removed edge to NULL.
        node->edgeNodes[edge_idx] = NULL;
        // Update function log information.
        updateLogInfo(node, fnId, REMOVAL);
    }
}

void edgeReplace(Node *node, ADDRINT value_id, int edge_idx, UINT32 fnId) {

    Node *to = IRGraph->nodes[value_id];
    ReplacedInfo replacedInfo;
    // Update node's 'replace' optimization information.
    if (node->edgeNodes[edge_idx] != NULL) {
        Node *from = node->edgeNodes[edge_idx];
        replacedInfo.nodeIdFrom = from->id;
        replacedInfo.nodeIdTo = to->id;
    }
    else {
        replacedInfo.nodeIdFrom = INT_INVALID;
        replacedInfo.nodeIdTo = to->id;
    }
    node->fnOrder2repInfo[IRGraph->fnOrderId] = replacedInfo;
    // Replace existing edge node with the node with 'value_id'.
    node->edgeNodes[edge_idx] = to;
    // Update function log information.
    updateLogInfo(node, fnId, REPLACE);
}

void edgeAddition(Node *node, ADDRINT location, ADDRINT value_id, UINT32 fnId) {

    Node *adding = IRGraph->nodes[value_id];
    // Update node's 'add' optimization information.
    node->fnOrder2addNodeId[IRGraph->fnOrderId] = adding->id;
    // Add an edge from 'this' node to the node with 'value_id'. 
    node->edgeNodes[node->numberOfEdges] = adding;
    node->edgeAddrs[node->numberOfEdges] = location;
    node->numberOfEdges++;
    // Update function log information.
    updateLogInfo(node, fnId, ADDITION);
}

void nodeDestroy(Node *node, UINT32 fnId) {

    node->alive = false;
    // Update function log information.
    updateLogInfo(node, fnId, KILL);
}

void directValueWrite(Node *node, ADDRINT location, ADDRINT value, UINT32 fnId) {
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
    // If is_written is false, then is a new value write. Thus, add the offset and value.
    if (!is_written) {
        // Update the directValOpt's valTo.
        directValOpt.valTo = value;
        // Update offsets and valuesInLocs.
        node->offsets[node->numberOfLocs] = offset;
        node->valuesInLocs[node->numberOfLocs] = value;
        node->numberOfLocs++;
    }
    node->fnOrder2dirValOpt[IRGraph->fnOrderId] = directValOpt;
    // Update function log information.
    updateLogInfo(node, fnId, VALUE_CHANGE);
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
 *  trace.out according to the Data Ops trace file format.
 * Output: None
 */
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

void write2Json() {
    ofstream jsonFile;
    jsonFile.open("ir.json");

    int counter = 0;

    jsonFile << "{" << endl;
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
        // Write added optimization information.
        counter = 0;
        jsonFile << "           \"added\": {" << endl;
        map<int,int>::iterator itAdd;
        for (itAdd = node->fnOrder2addNodeId.begin(); itAdd != node->fnOrder2addNodeId.end(); ++itAdd) {
            jsonFile << "               \"" << dec << itAdd->first << "\":" << itAdd->second;
            if (counter < int((node->fnOrder2addNodeId).size())-1) {
                jsonFile << "," << endl;
            }
            else {
                jsonFile << endl;
            }
            counter++;
        }
        jsonFile << "           }," << endl;
        // Write removed optimization information.
        counter = 0;
        jsonFile << "           \"removed\": {" << endl;
        map<int,int>::iterator itRem;
        for (itRem = node->fnOrder2remNodeId.begin(); itRem != node->fnOrder2remNodeId.end(); ++itRem) {
            jsonFile << "               \"" << dec << itRem->first << "\":" << itRem->second;
            if (counter < int((node->fnOrder2remNodeId).size())-1) {
                jsonFile << "," << endl;
            }
            else {
                jsonFile << endl;
            }
            counter++;
        }
        jsonFile << "           }," << endl;
        // Write replaced optimization information.
        counter = 0;
        jsonFile << "           \"replaced\": {" << endl;
        map<int,ReplacedInfo>::iterator itRep;
        for (itRep = node->fnOrder2repInfo.begin(); itRep != node->fnOrder2repInfo.end(); ++itRep) {
            jsonFile << "               \"" << dec << itRep->first << "\": {" << endl;
            jsonFile << "                   \"from\":" << (itRep->second).nodeIdFrom << "," << endl;
            jsonFile << "                   \"to\":" << (itRep->second).nodeIdTo << endl;

            if (counter < int((node->fnOrder2repInfo).size())-1) {
                jsonFile << "                }," << endl;
            }
            else {
                jsonFile << "                }" << endl;
            }
            counter++;
        }
        jsonFile << "           }," << endl;
        // Write direct value optimization information.
        counter = 0;
        jsonFile << "           \"directValueOpt\": {" << endl;
        map<int, DirectValOpt>::iterator itDir;
        for (itDir = node->fnOrder2dirValOpt.begin(); itDir != node->fnOrder2dirValOpt.end(); ++itDir) {
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

            if (counter < int((node->fnOrder2dirValOpt).size())-1) {
                jsonFile << "               }," << endl;
            }
            else {
                jsonFile << "               }" << endl;
            }
            counter++;
        }
        jsonFile << "           }," << endl;
        // Function access log information.
        counter = 0;
        jsonFile << "           \"fnAccess\": {" << endl;
        map<int, FnInfo>::iterator itFnInfo;
        for (itFnInfo = node->fnInfo.begin(); itFnInfo != node->fnInfo.end(); ++itFnInfo) {
            jsonFile << "               \"" << dec << itFnInfo->first << "\": {" << endl;
            jsonFile << "                   \"fnId\":" << dec << (itFnInfo->second).fnId << "," << endl;
            jsonFile << "                   \"type\":" << dec << (itFnInfo->second).accessType << endl;

            if (counter < int((node->fnInfo).size())-1) {
                jsonFile << "               }," << endl;
            }
            else {
                jsonFile << "               }" << endl;
            }
            counter++;

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
    counter = 0;
    jsonFile << "   \"fnId2Name\": {" << endl;
    map<UINT32, std::string>::iterator itFnId2Name;
    for (itFnId2Name = IRGraph->fnId2Name.begin(); itFnId2Name != IRGraph->fnId2Name.end(); ++itFnId2Name) {
        jsonFile << "       \"" << dec << itFnId2Name->first << "\":";
        jsonFile << "\"" << itFnId2Name->second << "\"";

        if (counter < int((IRGraph->fnId2Name).size())-1) {
            jsonFile << "," << endl;
        }
        else {
            jsonFile << endl;
        }
        counter++;
    }
    jsonFile << "   }" << endl;
    jsonFile << "}" << endl;
}

/**
 * Function: endFile
 * Description:
 * Output:
 **/
void endFile() {
    write2Json();
}
