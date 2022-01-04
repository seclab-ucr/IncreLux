CUR_DIR = $(shell pwd)
LLVM_SRC := ${CUR_DIR}/llvm/llvm-9.0.0.src
LLVM_BUILD := ${CUR_DIR}/llvm/build
INCRELUX_DIR := ${CURDIR}/src
INCRELUX_BUILD := ${CURDIR}/build

build_increlux_func = \
		     (mkdir -p ${2} \
		     && cd ${2} \
		     && PATH=${LLVM_BUILD}/bin:${PATH} \
		     LLVM_ROOT_DIR=${LLVM_BUILD}/bin \
		     LLVM_LIBRARY_DIRS=${LLVM_BUILD}/lib \
		     LLVM_INCLUDE_DIRS=${LLVM_BUILD}/include \
		     CC=clang  CXX=clang++ \
		     cmake ${1} -DCMAKE_BUILD_TYPE=Release \
		     -DCMAKE_CXX_FLAGS_RELEASE="-std=c++1y -frtti -fpic" \
		     && make -j${NPROC})

all: increlux

increlux:
	$(call build_increlux_func, ${INCRELUX_DIR}, ${INCRELUX_BUILD})
