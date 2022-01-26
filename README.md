# JITCIRModeler
Just-in-Time Compiler Intermediate Representation Modeler (JITCIRModeler)

## Overview

## Building the tools
1) Download Intel's [Pin](https://software.intel.com/content/www/us/en/develop/articles/pin-a-dynamic-binary-instrumentation-tool.html) dynamic instrumentation system and set the environment variable `PIN_ROOT` to point to the directory containing Pin. (Recommended Pin version is 3.13).
2) Set the environment variable `XED_ROOT` to point to the directory containing xed.
3) Clone the repository under the root directory of the Pin. This is where `PIN_ROOT` is pointing.
4) Execute the command under JITCIRModeler/IRModeler/.

    './build.sh'

## IRModeler Execute Command and Example

\<PIN\_ROOT\>/pin -t \<PIN\_ROOT\>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- \<Target Executable\> \<Input Program to the Target Executable\>

e.g.,
    \<PIN\_ROOT\>/pin -t \<PIN\_ROOT\>/JITCIRModeler/IRModeler/obj-intel64/IRModeler.so -- d8 poc.js
