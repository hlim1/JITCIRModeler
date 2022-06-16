# JITCIRModeler
Just-in-Time Compiler Intermediate Representation Modeler (JITCIRModeler)

## Overview

## Building the tools
    1) Download Intel's [Pin](https://software.intel.com/content/www/us/en/develop/articles/pin-a-dynamic-binary-instrumentation-tool.html) dynamic instrumentation system and set the environment variable `PIN_ROOT` to point to the directory containing Pin. (Recommended Pin version is 3.13).
    2) Set the environment variable `XED_ROOT` to point to the directory containing xed.
    3) Clone the repository under the root directory of the Pin. This is where `PIN_ROOT` is pointing.
    4) Execute the command under JITCIRModeler/IRModeler/.

    'make'

## IRModeler Execute Command and Example

    \<PIN\_ROOT\>/pin -t \<PIN\_ROOT\>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- \<Target Executable\> \<Input Program to the Target Executable\>

    e.g.,
    \<PIN\_ROOT\>/pin -t \<PIN\_ROOT\>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- d8 poc.js

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
            "updated_opcode_id":"opcode"
        },
        "added": {
            "fnOrderId":added node id number
        },
        "removed": {
            "fnOrderId":removed node id number
        },
        "replaced": {
            "fnOrderId": {
                "from":replaced node id number,
                "to":replacing node id number
            }
        },
        "directValueOpt": {
            "fnOrderId": {
                "offset":offset from the block head address,
                "valFrom":"value changed from",
                "valTo":"value changed to",
                "is_update":true or false
            }
        },
        "fnAccess": {
            "fnOrderId": {
                "fnId": function id number,
                "type": access type number
            }
        }
    }
    ],
    "fnId2Name": {
        "fnId": "function name"
    }
}
```

```"alive"```: Boolean to indicate whether the node was killed at the end of JIT compilation.

```"fnOrderId"```: Function execution order id.

```"is_update"```: Boolean to indicate whether the direct value optimization was for updating the existing value or adding a new value to the new location.

```"updated_opcode_id"```: Initial opcode has id=0. Each opcode update gets incremented id.
