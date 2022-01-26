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

#include <cstring>
#include <cassert>
#include <cstdio>
#include <iostream>

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

map<ADDRINT,MWInst> writes;
map<ADDRINT,MRInst> reads;
map<ADDRINT,MWInst> targetMWs;
map<int,SrcReg> targetSrcRegs;

SrcReg srcRegsHolder[MAX_REGS];
int regSize = 0;
UINT8  currentRaxVal[LARGEST_REG_SIZE];
UINT32 currentRaxValSize = 0;
bool populate_regs = false;
ADDRINT lastMemReadLoc = ADDRINT_INVALID;
int targetSrcRegsKey = 0;

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
    node->opcode = get_opcode(node, system_id);
    assert (node->opcode != ADDRINT_INVALID);

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

ADDRINT get_opcode(Node *node, UINT32 system_id) {

    ADDRINT opcode = ADDRINT_INVALID;

    if (system_id == V8) {
        opcode = get_opcode_v8(node);    
    }
    else if (system_id == JSC) {
        opcode = get_opcode_jsc(node);    
    }

    return opcode;
}

ADDRINT get_opcode_v8(Node *node) {

    ADDRINT value = ADDRINT_INVALID;
    map<ADDRINT,MWInst>::iterator it;
    for (it = targetMWs.begin(); it != targetMWs.end(); ++it) {
        if (it->first == node->intAddress) {
            value = (it->second).value;
            break;
        }
    }
    assert(value != ADDRINT_INVALID);

    ADDRINT opcode = ADDRINT_INVALID;
    map<ADDRINT,MWInst>::iterator it2;
    for (it2 = writes.begin(); it2 != writes.end(); ++it2) {
        MWInst write = it2->second;
        if (write.valueSize == V8_OPCODE_SIZE) {
            for (int i = 0; i < write.regSize; i++) {
                if (value == write.srcRegs[i].value) {
                    opcode = write.value;
                    break;
                }
            }
            if (opcode != ADDRINT_INVALID) {
                break;
            }
        }
    }

    return opcode;
}

ADDRINT get_opcode_jsc(Node *node) {

    ADDRINT opcode = ADDRINT_INVALID;

    map<ADDRINT,MRInst>::iterator it;
    for (it = reads.begin(); it != reads.end(); ++it) {
        MRInst read = it->second;
        string fn = strTable.get(read.fnId);
        if (fnInFormers(fn) && read.valueSize == JSC_OPCODE_SIZE) {
            for (int i = 0; i < read.regSize; i++) {
                if (
                        node->intAddress == read.srcRegs[i].value)
                {
                    opcode = read.value;
                    break;
                }
            }
            if (opcode != ADDRINT_INVALID) {
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
                bool is_direct = is_direct_assignment(node, write.value);
                if (is_direct) {
                    // Compute the distance between the block head and the written location,
                    // then write to node's occupiedLocs to track which locations are wrriten.
                    ADDRINT dis2Head = write.location - blockHead;
                    node->occupiedLocs[node->numberOfLocs] = dis2Head;
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

bool is_direct_assignment(Node *node, ADDRINT value) {

    bool is_direct_assignment = true;

    map<ADDRINT,MWInst>::iterator it;
    for (it = writes.begin(); it != writes.end(); ++it) {
        MWInst write = it->second;
        if (value == write.location) {
            is_direct_assignment = false;
            break;
        }
    }

    return is_direct_assignment;
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

int get_edge_idx(Node *node, ADDRINT address) {

    int edge_idx = INT_INVALID;

    for (int i = 0; i < node->numberOfEdges; i++) {
        if (address == node->edgeAddrs[i]) {
            edge_idx = i;
            break;
        }
    }

    return edge_idx;
}

// Functions for prints for debugging.

void printUINT8(UINT8* arr, UINT32 size) {

    for (UINT32 i = size-1; i > 0; i--) {
        printf("%02x", arr[i]);
    }
    printf("%02x\n", arr[0]);
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
    cout << "Written values (Distance from block head -> value):" << endl;
    for (int i = 0; i < node->numberOfLocs; i++) {
        cout << "+" << dec << node->occupiedLocs[i] << " -> ";
        if (node->valuesInLocs[i] != ADDRINT_INVALID) {
            cout << hex << node->valuesInLocs[i] << endl;
        }
        else {
            cout << hex << ADDRINT_INVALID << endl;
        }
    }
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
bool analyzeRecords(THREADID tid, const CONTEXT *ctx, UINT32 fnId, UINT32 opcode, bool is_range) {

    ThreadData &data = tls[tid];

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
        analyzeMemWrites(tid, fnId, is_range);
        data.memWriteSize = 0; 
    }

    // Keep tracks of source register information of the node allocator function instructions
    // separately.
    if (fnInFormers(fn) || fnInAllocs(fn)) {
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

    if (IRGraph->lastNodeId > 0) {
        trackOptimization(write.location, write.value);
    }
}

void trackOptimization(ADDRINT location, ADDRINT value) {

    // Check if the current memory location belongs to any one of existing node.
    ADDRINT nodeId = ADDRINT_INVALID;
    bool is_node = false;
    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        Node *node = IRGraph->nodes[i];
        if (location > node->blockHead && location < node->blockTail) {
            nodeId = node->id;
            if (location == node->intAddress) {
                is_node = true;
            }
            break;
        }
    }

    // If the location belongs to one of the nodes,
    if (nodeId != ADDRINT_INVALID) {
        Node *node = IRGraph->nodes[nodeId];
        // Check if the memory write value is an address of a node.
        ADDRINT value_id = compareValuetoIRNodes(value);
        // Check if the location is already occupied edge.
        int edge_idx = get_edge_idx(node, location);
        // If the location is already occupied edge, handle edge removal or edge replace.
        if (edge_idx != INT_INVALID) {
            // If value is '0', which is to wipe out the memory location, handle edge 'removal'.
            if (value == WIPEMEM) {
                Node *target = node->edgeNodes[edge_idx];
                // In some cases, I see an instruction that cleans memory location where it's alreay
                // wiped out with '0' and never written with other value. Such instruction will clean
                // the node's edge (already to get NULL). Thus, the earlier line will return NULL.
                // We only count the edge removal only if the edge has a valid node, so we handle
                // this case by checking that the returned value from the earlier line is NOT NULL.
                if (target != NULL) {
                    // Update 'this' node's 'remove' optimization information.
                    node->removedNodeIds[node->numberOfRemoves] = target->id;
                    node->removedEdgeIdx[node->numberOfRemoves] = edge_idx;
                    node->numberOfRemoves++;

                    // Set the removed edge to NULL.
                    node->edgeNodes[edge_idx] = NULL;
                }
            }
            // If value is one of the existing nodes, handle edge 'replace'.
            else if (value_id != ADDRINT_INVALID) {
                Node *to = IRGraph->nodes[value_id];
                // Update 'this' node's 'replace' optimization information.
                if (node->edgeNodes[edge_idx] != NULL) {
                    Node *from = node->edgeNodes[edge_idx];
                    node->replacedNodeIds[node->numberOfReplaces][0] = from->id;
                    node->replacedNodeIds[node->numberOfReplaces][1] = to->id;
                }
                else {
                    node->replacedNodeIds[node->numberOfReplaces][0] = INT_INVALID;
                    node->replacedNodeIds[node->numberOfReplaces][1] = to->id;
                }
                node->numberOfReplaces++;

                // Replace existing edge node with the node with 'value_id'.
                node->edgeNodes[edge_idx] = to;
            }
        }
        // If the location is not occupied edge, but the value is a node, handle edge 'addition'.
        else if (edge_idx == INT_INVALID && value_id != ADDRINT_INVALID) {
            Node *adding = IRGraph->nodes[value_id];
            // Update 'this' node's 'add' optimization information.
            node->addedNodeIds[node->numberOfAdds] = adding->id;
            node->numberOfAdds++;

            // Add an edge from 'this' node to the node with 'value_id'. 
            node->edgeNodes[node->numberOfEdges] = adding;
            node->edgeAddrs[node->numberOfEdges] = location;
            node->numberOfEdges++;
        }
        // If the location belongs to some node block, but it's not an edge and the value is not a node.
        else if (edge_idx == INT_INVALID && value_id == ADDRINT_INVALID) {
            // Node's are being destroyed and the location is being wiped by writing '0'.
            // We check such pattern in the instruction and mark the node dead.
            if (is_node && value == WIPEMEM) {
                node->alive = false;
            }
        }
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

/**
 * Function: endFile
 * Description:
 * Output:
 **/
void endFile() {
    
    for (int i = 0; i < IRGraph->lastNodeId; i++) {
        Node *node = IRGraph->nodes[i];
        printNode(node);
    }
}
