# IncreLux
Progressive Scrutiny: Incremental Detection of UBI bugs in the Linux Kernel

## Files
|  
|-src: Incremental analysis code for IncreLux  \
|-KLEE: Under constraint symbolic execution code and z3 as the contraint solver  \
|-llvm: The source code for llvm, clang and the basic block rename pass\*, we use llvm-9.0.0 and clang 9.0.0  \
|-example: An example showcase how Increlux works in below  \
|-Makefile: Used to compile IncreLux in src/  \
|-path\_verify.py: Wrapper to run KLEE \
|-getbclist.py: A wrapper to rename the basic block and collect the LLVM bitcode files for OS kernels \

## Installation:
### Installation with cmake, and it's been tested on Ubuntu 20.04.
```sh
    #change to the code folder
    $cd IncreLux
    $sh setup.sh
```
Now the ready to use binaries are path/to/IncreLux/build/bin/increlux and path/to/IncreLux/KLEE/klee/build/bin/klee

## How to use IncreLux
This section shows the essential steps to apply IncreLux to the target code, we will have a detailed step by step tutorial with a concrete example in as follows.
* Compile the target code with options: -O0, -g, -fno-short-wchar
* Rename the basic block and generate bitcode.list by the wrapper getbclist.py
```sh
    $python getbclist.py abs/dir/to/llvm
```
