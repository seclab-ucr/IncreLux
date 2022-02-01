FROM ubuntu:20.04
MAINTAINER Yizhuo Zhai <yizh.zhai@gmail.com>
WORKDIR /home/IncreLux
COPY . /home/IncreLux

RUN apt-get update -y
RUN apt-get install -y build-essential curl libcap-dev git cmake libncurses5-dev python-minimal python-pip unzip libtcmalloc-minimal4 && \
    apt-get install -y zlib1g-dev libgoogle-perftools-dev && \
    apt-get install -y libboost-all-dev &&\
    DEBIAN_FRONTEND=noninteractive apt -y --no-install-recommends install sudo emacs-nox vim-nox file python3-dateutil 

RUN cd llvm && mkdir build && cd build &&\
    cmake -G "Unix Makefiles" -DLLVM_ENABLE_RTTI=ON -DCMAKE_BUILD_TYPE=Release ../llvm-9.0.0.src &&\
    make -j4
RUN export LLVM_SRC=/home/IncreLux/llvm/llvm-9.0.0.src && \
    export LLVM_DIR=/home/UBITect/llvm/build && \
    export PATH=$LLVM_DIR/bin:$PATH && \
RUN cd ../../ &&\
    cd KLEE/z3 && \
    python scripts/mk_make.py && \
    cd build && \
    make && \
    make install
RUN cd ../../klee && \
    mkdir build && \
    cd build && \
    cmake -DENABLE_SOLVER_Z3=ON -DENABLE_UNIT_TESTS=OFF -DENABLE_SYSTEM_TESTS=OFF -DLLVM_CONFIG_BINARY=../../../llvm/build/bin/llvm-config -DLLVMCC=../../../llvm/build/bin/clang -DLLVMCXX=../../../llvm/build/bin/clang++  .. && \
    make  && \
    pip install psutil

COPY . .
