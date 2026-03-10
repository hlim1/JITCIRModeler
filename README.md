# JITCIRModeler
Just-in-Time Compiler Intermediate Representation Modeler (JITCIRModeler)

## Abstract
Just-in-Time (JIT) compilers are widely used to improve the performance of interpreter-based language implementations by creating optimized code at runtime. However, bugs in the JIT compiler’s code manipulation and optimization can result in the generation of incorrect code. Such bugs can be difficult to diagnose and fix, and can result in exploitable vulnerabilities. Unfortunately, existing approaches to automatic bug localization do not carry over well to such bugs. This paper discusses a different approach to analyzing JIT compiler optimization behaviors, based on using dynamic analysis to construct abstract models of the JIT compiler’s optimizer and back end. By comparing the models obtained for buggy and non-buggy executions of the JIT compiler, we can pinpoint the components of the JIT compiler’s internal representation that have been affected by the bug; this can then be mapped back to identify the buggy code. Our experiments with two real bugs for Google V8 JIT compiler, TurboFan, show the utility and practicality of our approach.

------------------------------------------------------------------------

## Publication

HeuiChan Lim, Xiyu Kang, and Saumya Debray. 2022. Modeling code manipulation in JIT compilers. In Proceedings of the 11th ACM SIGPLAN International Workshop on the State Of the Art in Program Analysis (SOAP 2022). Association for Computing Machinery, New York, NY, USA, 9–15. https://doi.org/10.1145/3520313.3534656

------------------------------------------------------------------------

## Requirements:

### Linux
- Intel's [Pin Tool](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html)
    - Stable version 4.1
    - Set the environment variable `PIN_ROOT` to point to the directory containing Pin.
- Intel's [XED](https://intelxed.github.io/) disassembler.
    - Set the environment variable `XED_ROOT` to point to the directory containing xed.

### Windows
- Not Supported.

### MacOS
- Not Supported.

------------------------------------------------------------------------

## How to Install
1. Install the required software.
2. Clone the repository to the local directory.
```
$git clone git@github.com:hlim1/JITCIRModeler.git
```
3. Navigate to the `JITCIRModeler/IRModeler` directory.
4. Make
```
$make
```

------------------------------------------------------------------------

## IRModeler Execute Command and Example

```
<PIN_ROOT>/pin -t <PIN_ROOT>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- <Target Executable> <Input Program to the Target Executable>

e.g.,
  <PIN_ROOT>/pin -t <PIN_ROOT>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- d8 poc.js
```

------------------------------------------------------------------------

## Output
    Formated JSON file - ir.json

------------------------------------------------------------------------

## JSON File Specification

This project produces a file named **`ir.json`** that records the
**intermediate representation (IR) graph** and execution information
observed during JIT compilation.

The JSON file contains:

-   IR node information
-   optimization logs
-   instruction access logs
-   function mappings
-   optional memory access logs

The structure is designed to allow reconstruction of how the IR graph
evolves during compilation.

------------------------------------------------------------------------

### Top-Level Structure

The JSON file contains the following top-level fields:

``` json
{
  "nodes": [...],
  "fnId2Name": {...},
  "fnCallRetId2fnId": {...},
  "memory_writes": {...},
  "memory_reads": {...}
}
```

  `nodes`                List of IR nodes

  `fnId2Name`            Mapping from function ID to function name

  `fnCallRetId2fnId`     Mapping from function call-return ID to function
                         ID

  `memory_writes`        Memory writes during compilation (DEBUG mode
                         only)

  `memory_reads`         Memory reads during compilation (DEBUG mode
                         only)

------------------------------------------------------------------------

### Nodes

Each element in `nodes` represents a single IR node in the graph.

Example:

``` json
{
  "id": 42,
  "alive": true,
  "address": "7f31ac40",
  "opcode": "34",
  "is_nonIR": false,
  "size": 24,
  "edges": [11, 17],
  "initialEdges": [11, 17],
  "directValues": {},
  "opcode_log": {},
  "added": {},
  "removed": {},
  "replaced": {},
  "directValueOpt": {},
  "evaluates": {},
  "instAccess": {}
}
```

  `id`                   Unique identifier of the node

  `alive`                Indicates whether the node is still active in
                         the graph

  `address`              Node memory address (hex string, only when
                         `DEBUG_JSON` is enabled)

  `opcode`               Opcode of the node (hex string)

  `is_nonIR`             Indicates a non-IR node (only when compiled with
                         `SPM`)

  `size`                 Size of the node

  `edges`                List of node IDs that the current node has a direct edge connection (shows the final edge information).

  `initialEdges`         List of node IDs that the current node had an initial direct edge connection (shows the original edge information).

  `directValues`         Values stored directly inside the node

  `opcode_log`           History of opcode updates

  `added`                Nodes added as edges during optimization

  `removed`              Nodes removed during optimization

  `replaced`             Edge replacements during optimization

  `directValueOpt`       Direct value modifications

  `evaluates`            Value evaluations of node fields

  `instAccess`           Instruction access log

------------------------------------------------------------------------

### Edges

``` json
"edges": [12, 15, 15]
```

The `edges` array contains **IDs of input nodes**.

The **position of each element corresponds to the edge position**.

Duplicate node IDs may appear if multiple edges reference the same
node.

------------------------------------------------------------------------

### Direct Values

``` json
"directValues": {
  "16": "3ff00000"
}
```

Records values stored directly in the node's memory layout.

  key     Byte offset within the node
  
  value   Value stored at that offset (hex string)

------------------------------------------------------------------------

### Opcode Log

``` json
"opcode_log": {
  "10423": "3a"
}
```

Records opcode updates performed during execution.

  key     Instruction ID
  
  value   Updated opcode

------------------------------------------------------------------------

### Optimization Logs

#### Added

``` json
"added": {
  "10512": {
    "nodeId": 77,
    "position": 1
  }
}
```

A new edge was added.

  `nodeId`     ID of the added node
  
  `position`   Position (index) in the `edges` array

------------------------------------------------------------------------

#### Removed

``` json
"removed": {
  "10620": {
    "nodeId": 19,
    "position": 0
  }
}
```

An existing edge was removed.

  `nodeId`     Remove ID of the node
  
  `position`   Position (index) in the `edges` array

------------------------------------------------------------------------

#### Replaced

``` json
"replaced": {
  "10645": {
    "from": 19,
    "to": 25,
    "position": 0
  }
}
```

An edge was replaced.

  `from`       Original node ID
  
  `to`         Replacement node ID
  
  `position`   Position (index) in the `edges` array

------------------------------------------------------------------------

### Direct Value Optimizations

``` json
"directValueOpt": {
  "11002": {
    "offset": 16,
    "valFrom": "1",
    "valTo": "0",
    "is_update": true
  }
}
```

Records modifications to values stored inside nodes.

  `offset`      Offset from node base
  
  `valFrom`     Previous value
  
  `valTo`       New value
  
  `is_update`   Indicates update vs insertion

------------------------------------------------------------------------

### Evaluation Records

``` json
"evaluates": {
  "11123": {
    "offset": 16,
    "value": "3ff00000"
  }
}
```

Records values read from the node during execution.

    `offset`      Offset from node base
    
    `value`      Value in the offset location

------------------------------------------------------------------------

### Instruction Access Log

``` json
"instAccess": {
  "11201": {
    "fnCallRetId": 53,
    "fnId": 12,
    "PhaseFnId": 104,
    "binary": "48 89 d8",
    "type": 1
  }
}
```

Logs instructions that accessed the node.

  `fnCallRetId`   Unique identifier of a function call-return
  
  `fnId`          Function ID
  
  `PhaseFnId`     JIT Compiler optimization phase function ID
  
  `binary`        Instruction opcode and operands (DEBUG mode only)
  
  `type`          Access type identifier

#### Access Types

The `type` field in `instAccess` corresponds to the following access categories:

| Value | Name | Description |
|------|------|-------------|
| -1 | INVALID | Invalid or uninitialized access |
| 0 | ADDITION | An edge was added to the node |
| 1 | REMOVAL | An edge was removed from the node |
| 2 | REPLACE | An existing edge was replaced with another node |
| 3 | KILL | The node was killed (removed from the IR graph) |
| 4 | VALUE_CHANGE | A direct value stored in the node was updated |
| 5 | EVALUATE | The node was read or evaluated |
| 6 | OP_UPDATE | The node's opcode was updated |
| 7 | CREATE | The node was created |

------------------------------------------------------------------------

### Function Information

#### fnId2Name

``` json
"fnId2Name": {
  "12": "v8::internal::compiler::GraphBuilderPhase::Run"
}
```

Maps internal **function IDs to function names**.

------------------------------------------------------------------------

#### fnCallRetId2fnId

``` json
"fnCallRetId2fnId": {
  "53": 12
}
```

Maps **function call-return identifiers to function IDs**.

------------------------------------------------------------------------

### Memory Access Logs (DEBUG Mode)

These sections are only present when compiled with **`DEBUG_JSON`**.

------------------------------------------------------------------------

#### Memory Writes

``` json
"memory_writes": {
  "7ffd3321": "3ff00000"
}
```

Records memory writes that occurred during JIT compilation.

------------------------------------------------------------------------

#### Memory Reads

``` json
"memory_reads": {
  "7ffd3321": "3ff00000"
}
```

Records memory reads that occurred during JIT compilation.

------------------------------------------------------------------------

### Notes

-   Many numeric values are stored as **hexadecimal strings** because
    they represent raw memory or opcode values.
-   The `edges` array preserves operand ordering.
-   Duplicate edge targets are allowed.
-   Sections controlled by `DEBUG_JSON` are intended for debugging and
    analysis.
