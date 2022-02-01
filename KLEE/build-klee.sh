#!/bin/bash -e

CUR_DIR=$PWD

cd z3
python scripts/mk_make.py --prefix=${CUR_DIR}/z3/prefix
if [ ! -d "build" ]; then
	mkdir build
fi
cd build
make -j32
make install

export Z3PATH=${CUR_DIR}/z3/prefix
export Z3_INCLUDE_DIRS=${CUR_DIR}/z3/build/include


cd ../../klee
if [ ! -d "build" ]; then
	mkdir build
fi
mkdir build
cd build
cmake -DENABLE_UNIT_TESTS=OFF -DENABLE_SYSTEM_TESTS=OFF -DLLVM_CONFIG_BINARY=../../../llvm/build/bin/llvm-config -DLLVMCC=../../../llvm/build/bin/clang -DLLVMCXX=../../../llvm/build/bin/clang++ -DENABLE_SOLVER_Z3=ON -DZ3_INCLUDE_DIRS=${CUR_DIR}/z3/prefix/include -DZ3_LIBRARIES=${CUR_DIR}/z3/prefix/lib/libz3.so "Unix Makefiles" ..
make -j32
