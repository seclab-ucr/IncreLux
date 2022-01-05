#!/bin/bash

apt-get update -y
apt-get install -y build-essential curl libcap-dev git cmake libncurses5-dev python-minimal python-pip unzip libtcmalloc-minimal4
apt-get install -y zlib1g-dev libgoogle-perftools-dev
DEBIAN_FRONTEND=noninteractive apt -y --no-install-recommends install sudo emacs-nox vim-nox file python3-dateutil
apt-get install -y libboost-all-dev
apt-get install -y build-essential cmake

cd llvm
sh ./build.sh

cd ../KLEE
sh ./build-klee.sh

cd ..
make -j32
