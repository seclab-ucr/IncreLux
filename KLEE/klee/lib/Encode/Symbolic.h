/*
 * Symbolic.h
 *
 *  Created on: Oct 22, 2018
 *      Author: klee
 */

#ifndef LIB_ENCODE_SYMBOLIC_H_
#define LIB_ENCODE_SYMBOLIC_H_

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "../../include/klee/ExecutionState.h"
#include "../../include/klee/Expr/Expr.h"
#include "../../include/klee/util/Ref.h"

namespace llvm {
class Function;
} /* namespace llvm */

using namespace llvm;

namespace klee {

class Symbolic {
public:
	Symbolic(Executor *executor);
	virtual ~Symbolic();

private:
	std::stringstream ss;
	std::map<uint64_t, unsigned> loadRecord;
	std::map<uint64_t, unsigned> storeRecord;
	Executor* executor;
	std::string json;
public:
	void load(ExecutionState &state, KInstruction *ki);
	void callReturnValue(ExecutionState &state, KInstruction *ki, Function *function);
	void Alloca(ExecutionState &state, KInstruction *ki, unsigned size);
    static std::string getInst(llvm::Instruction *inst);
    static void isWarning(ExecutionState &state, KInstruction *ki);
    static bool isInstSame(std::vector<std::string> inst1, std::vector<std::string> inst2);
    int checkInst(ExecutionState &state, KInstruction *ki);
    int isAllocaOrInput(ref<Expr> v);
    std::string getName(ref<klee::Expr> value);
    void resolveSymbolicExpr(ref<klee::Expr> symbolicExpr, std::set<std::string> &relatedSymbolicExpr);

    std::string inputName = "input";
    uint64_t inputCount = 0;
    std::string allocaName = "alloca";
    uint64_t allocaCount = 0;

private:
	ref<Expr> manualMakeSymbolic(std::string name, unsigned size);
	ref<Expr> readExpr(ExecutionState &state, ref<Expr> address,
			Expr::Width size);
	unsigned getLoadTime(uint64_t address);
	unsigned getStoreTime(uint64_t address);
	std::string createGlobalVarFullName(std::string i, unsigned memoryId,
			uint64_t address, bool isGlobal, unsigned time, bool isStore);

};

} /* namespace klee */

#endif /* LIB_ENCODE_SYMBOLIC_H_ */
