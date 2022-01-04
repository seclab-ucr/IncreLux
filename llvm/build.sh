#!/bin/bash -e

if [ ! -d "build" ]; then
	  mkdir build
fi

cd build
cmake -G "Unix Makefiles" -DLLVM_ENABLE_RTTI=ON -DCMAKE_BUILD_TYPE=Release ../llvm-9.0.0.src
make -j32
