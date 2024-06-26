# JITCIRModeler
Just-in-Time Compiler Intermediate Representation Modeler (JITCIRModeler)

## Abstract
Just-in-Time (JIT) compilers are widely used to improve the performance of interpreter-based language implementations by creating optimized code at runtime. However, bugs in the JIT compiler’s code manipulation and optimization can result in the generation of incorrect code. Such bugs can be difficult to diagnose and fix, and can result in exploitable vulnerabilities. Unfortunately, existing approaches to automatic bug localization do not carry over well to such bugs. This paper discusses a different approach to analyzing JIT compiler optimization behaviors, based on using dynamic analysis to construct abstract models of the JIT compiler’s optimizer and back end. By comparing the models obtained for buggy and non-buggy executions of the JIT compiler, we can pinpoint the components of the JIT compiler’s internal representation that have been affected by the bug; this can then be mapped back to identify the buggy code. Our experiments with two real bugs for Google V8 JIT compiler, TurboFan, show the utility and practicality of our approach.

## Publication

HeuiChan Lim, Xiyu Kang, and Saumya Debray. 2022. Modeling code manipulation in JIT compilers. In Proceedings of the 11th ACM SIGPLAN International Workshop on the State Of the Art in Program Analysis (SOAP 2022). Association for Computing Machinery, New York, NY, USA, 9–15. https://doi.org/10.1145/3520313.3534656

## Requirements:
### Linux
- Intel's [Pin Tool](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html)
    - Stable Version for BackEndModeler is 3.13.
    - Set the environment variable `PIN_ROOT` to point to the directory containing Pin.
- Intel's [XED](https://intelxed.github.io/) disassembler.
    - Set the environment variable `XED_ROOT` to point to the directory containing xed.

### Windows
- Not Supported.

### MacOS
- Not Supported.

### How to Install
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

## IRModeler Execute Command and Example

```
<PIN_ROOT>/pin -t <PIN_ROOT>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- <Target Executable> <Input Program to the Target Executable>

e.g.,
  <PIN_ROOT>/pin -t <PIN_ROOT>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- d8 poc.js
```

## Output
    Formated JSON file - ir.json

## JSON File Specification

```
{
    "nodes"[
    {
        "id":node id number,
        "alive":true or false,
        "address":"node address",
        "opcode":"node opcode",
        "is_cfgBlock":true or false,
        "size":node size number,
        "edges":[list of edge node ids],
        "directValues": {
            "offset":"value"
        },
        "opcode_update": {
            "instruction id":"opcode"
        },
        "added": {
            "instruction id":added node id number
        },
        "removed": {
            "instruction id":removed node id number
        },
        "replaced": {
            "instruction id": {
                "from":replaced node id number,
                "to":replacing node id number
            }
        },
        "directValueOpt": {
            "instruction id": {
                "offset":offset from the block head address,
                "valFrom":"value changed from",
                "valTo":"value changed to",
                "is_update":true or false
            }
        },
        "instAccess": {
            "instruction id": {
                "fnCallRetId": function call-return id,
                "fnId": function id,
                "binary": instruction opcode & operands binary,
                "type": access type number
            }
        }
    }
    ],
    "fnId2Name": {
        "fnId": "function name"
    },
    "fnCallRetId2fnId" {
      "fnCallRetId":fnId
    }
}
```

```"alive"```: Boolean to indicate whether the node was killed at the end of JIT compilation.

```"is_update"```: Boolean to indicate whether the direct value optimization was for updating the existing value or adding a new value to the new location.

```"fnCallRetId"```: Function call-return order id.

```"instruction id"```: Executed instruction id. 
