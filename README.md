# JITCIRModeler
Just-in-Time Compiler Intermediate Representation Modeler (JITCIRModeler)

## IRModeler Execute Command and Example

```
<PIN_ROOT>/pin -t <PIN_ROOT>/AutoLocalizer/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- <Target Executable> <Input Program to the Target Executable>

e.g.,
  <PIN_ROOT>/pin -t <PIN_ROOT>/AutoLocalizer/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- d8 poc.js
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
