# Tracer #

Welcome! In this folder you will find the code necessary to build the tracer. This portion of the overall toolset is used to gather a trace of an executable when can then be analyzed using other portions of the toolset. It uses a program from Intel called PIN to run and analyze the binary, then prints out a trace in a binary format.

## Code Organization ##
For those of you brave enough to read or modify this code, there are a few important things to know about the current organization. To improve runtime, Pin has 2 phases: the instrumentation and the analysis phases. While these do make runtime faster, they can cause quite a bit of confusion. The instrumentation phase of the computation (which is defined in DataOpsMain.cpp) is only supposed to be executed once per instruction. In this phase, PIN intends the user to gather whatever information they need about an instruction that is not dependent on runtime. In addition, the user is supposed to register callback functions to the analysis routines, which we will discuss soon. In the tracer, we gather information about an instruction's: source registers, destination registers, instruction origin, and function. This information is then provided to the analysis functions.

The analysis functions (DataOpsTrace.cpp) are intended to run every time an instruction executes. As a result, we want to do as little processing in here as possible. In some cases, this is not possible. Consider processing memory reads/writes for example. The address for these is (usually) not known until runtime, so all of this processing happens in the analysis functions. Another thing that will probably be noticed is that there are a lot of small functions. This was done to allow PIN to inline the analysis routine, and provide flexibility (We can choose what we want to execute, allowing us to turn some things off). 

## Prerequisites ##

Linux

* gcc 3.4 or later
* clang++
* Intel Pin 3.8+

Windows (Currently Broken, needs to be updated with a library to read PE files)

* ~~Visual Studios 2008, 2010 or 2012~~
* ~~Cygwin with the make package from devel~~
* ~~Intel Pin 3.8+~~

## Building ##

### Linux ###
1. Download Pin 3.8+ from [Pin's download page](https://software.intel.com/en-us/articles/pintool-downloads).
2. Extract Pin from the archive.  Set the environment variable `PIN_ROOT` to point to Pin's base directory.  This directory will henceforth be referred to as `$PIN_ROOT`.
3. Checkout the toolset into an appropriate directory.  We will refer to this directory as `TOOLS_DIR`.
5. Navigate to the directory `$TOOLS_DIR/tracer`.
6. Run build.sh to build both the x86-64 and x86 versions of the tracer. 

### Windows ###
(Currently Broken, needs to be updated with a library to read PE files)
1. ~~Download and install Visual Studios 2008, 2010 or 2012~~
2. ~~Download and install Cygwin. Be sure to install Cygwin with make from devel and git.~~
3. ~~Add the Cygwin bin directory to Window’s path environment variable (make sure to log out and log back in or restart after adding the variable)~~
4. ~~Download Pin 3.6+ from [Pin's download page](https://software.intel.com/en-us/articles/pintool-downloads). Note that on a Windows machine, you need visual studios to build the pintool, so make sure to download the version of PIN that corresponds with your version of Visual Studios.~~
5. ~~Extract Pin from the archive. Set the environment variable `PIN_ROOT` to point to Pin's base directory.  This directory will henceforth be referred to as `$PIN_ROOT`.~~
7. ~~Checkout the toolset into an appropriate directory.~~
8. ~~If you want to trace a x86-64 program, open the Visual Studio x64 Win64 Command Prompt, while if you want to trace a x86 program, open the Visual Studio Command Prompt (note, not 64).~~
9. ~~In the command prompt, navigate to the tracer subdirectory of the toolset.~~
10. ~~Run build.cmd to build both the x86-64 and x86 versions of the tracer.~~

## Running ##

A trace can be created for an executable by running the following command, where `$ARCH` is either `obj-ia32` (for the IA-32 architecture) or `obj-intel64` (for the x86-64 architecture):

`$PIN_ROOT/pin [pin_options] -t $TOOLS_DIR/tracer/$ARCH/$TRACER_LIB [tool_options] -- <program> [program_options]`

Variable         | Description
-----------------|------------
$PIN_ROOT        | The base directory for Pin
pin_options      | Any [Pin command line switches](https://software.intel.com/sites/landingpage/pintool/docs/67254/Pin/html/group__KNOBS.html) to be enabled. Note, if there is self-modifying code, be sure to use the smc_strict command line switch.
$ARCH            | The name of the obj directory that corresponds to the architecture of the program to be traced. For x86, use obj-ia32 and for x86-64 use obj-intel64.
$TRACER_LIB      | The name of the tracer's dynamically linked library. On Windows, the name is Tracer.dll while on Linux, the name is Tracer.so.
tool_options     | Any options to provide to the tracer pintool (these are different from the options to provide to pin). The tracer's command line options are given below.
program          | The program to be traced.
program_options  | Any command line options to provide to the program to be traced.

## Tracer Command Line Options ##
All of the tracer's command line arguements are intended to allow the user to select what data they want to appear in a trace. By default all of the data requried to capture a full trace of the executable's behavior are turned on. There are also 2 options that are turned off by default because they technically don't provide any information that hasn't been seen before, but they are useful for debugging purposes. If available, for example, the reader will use the given sources to determine if it's internal state is consisten with the machine's state.


Option        | Description
--------------|------------
   nosrcid    | Do not include the instruction's source id in the trace. As a result an analysis routine will not be able to determine if an instruction is from the executable or a library.
   nofnid     | Do not include the instruction's function id in the trace. As a result an analysis routine will not be able to determine what function an instruction belongs to (such as main).
   noaddr     | Do not include the instruction's address in the trace. As a result an analysis routine will not be able to determine when instructions are being re-executed, or when code is read or written.
   nobin      | Do not include the instruction's binary in the trace. As a result an analysis routine will not know exactly what instruction executed.
   srcreg     | Include the values of an instruction's source registers in the trace. This is useful for debugging purposes.
   memread    | Include the values of an instruction's memory reads in the trace. This is useful for debugging purposes.
   nodestreg  | Do not include the values of an instruction's destination registers in the trace.
   nomemwrite | Do not include the values of an instruction's memory writes in the trace.

## Outputs ##
Three files will be outputted by the tracer. Descriptions are given below.

File         | Description
-------------|------------
trace.out    | The trace!
errors.out   | A file containing a list of any errors that occurred in the tracer pintool. These errors could affect the accuracy of the trace.
data.out     | A temporary file used by the tracer to print out the data section of a trace. This is appended to the end of the trace so it can be removed if desired.
