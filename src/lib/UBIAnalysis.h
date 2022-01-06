/// Created by ubuntu on 10/22/17.


//

#ifndef UBIANALYSIS_H
#define UBIANALYSIS_H


#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm/Support/CommandLine.h"

#include <set>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <queue>
#include <map>



#include "Common.h"
#include "StructAnalyzer.h"
#include "FunctionSummary.h"

//#define TEST

//
// typedefs
//
typedef std::vector< std::pair<llvm::Module*, llvm::StringRef> > ModuleList;
// Mapping module to its file name.
typedef std::unordered_map<llvm::Module*, llvm::StringRef> ModuleNameMap;
// The set of all functions.
typedef llvm::SmallPtrSet<llvm::Function*, 8> FuncSet;
// Mapping from function name to function.
typedef std::unordered_map<std::string, llvm::Function*> NameFuncMap;
typedef std::unordered_map<size_t, llvm::Function*> localFuncMap;
typedef llvm::SmallPtrSet<llvm::CallInst*, 8> CallInstSet;
typedef std::unordered_map<std::string, llvm::GlobalVariable*> GObjMap;
typedef llvm::DenseMap<llvm::Function*, CallInstSet> CallerMap;
typedef llvm::DenseMap<llvm::CallInst *, FuncSet> CalleeMap;
//mapping from the function to the functions it calls
typedef std::unordered_map<llvm::Function*, std::set<llvm::Function*>> CallMap;
//typedef llvm::DenseMap<llvm::Function *, FuncSet> CallMap;
typedef llvm::DenseMap<llvm::Function *, FuncSet> CalledMap;
typedef std::unordered_map<std::string, FuncSet> FuncPtrMap;

typedef std::unordered_map<llvm::Function *, Summary> FtSummary;
//typedef std::unordered_map<llvm::Function *, std::set<std::string>> FToBCMap;
typedef std::unordered_map<llvm::Function*, std::set<llvm::Value*>> FuncHolder;
typedef std::map<llvm::Type*, std::string> TypeNameMap;

struct GlobalContext {
    int order=0;
    //std::string jsonfile = "/data/home/yizhuo/incExp/minitest/kernelDiff/warn-415.json";
#ifdef TEST
    std::string jsonfile = "/data/home/yizhuo/incExp/minitest/10files/topology/warn-414.json";
    std::string jsonInc = "/data/home/yizhuo/incExp/minitest/10files/topology/warn-415.json";
#else
    //std::string jsonfile = "/data/home/yizhuo/incExp/minitest/kernelDiff/warn414-ret2.json";
    //std::string jsonfile = "/home/yizhuo/experiment/lll-414/warn414-0426.json";
    //std::string jsonInc = "/data/home/yizhuo/experiment/lll-414-1/warn4141.json";
    //std::string jsonfile = "/home/yizhuo/experiment/lll-56/warn0509.json";
    //std::string jsonInc = "/data/home/yizhuo/incExp/minitest/kernelDiff/warn-4141-inc.json";
    //std::string jsonInc = "/data/home/yizhuo/experiment/lll-56-7/warninc2.json";
    //std::string jsonfile = "/home/yizhuo/experiment/PerPatchTest/lll-414-def/warn.json";
    std::string absPath = "/home/yzhai003/inc-experiment/lll-v4.14/";
    std::string outFolder = "/home/yzhai003/IncreLux/example/v4.14/ubi-ia-out/";
    std::string incFolder = "/home/ubuntu/experiment/lll-v5.6.2/ia-out/";
    std::string newVersion = "lll-v5.6.2_";
    std::string oldVersion = "lll-v5.6.1_";

    std::string jsonFile = outFolder+"/ubiWarn.json";
    std::string ioJsonFile = outFolder+"/ioWarn.json";
    std::string oobJsonFile = outFolder+"/oobWarn.json";

    std::string jsonInc = incFolder+"/ubiIncWarn.json";
    std::string ioJsonIncFile = incFolder+"/ioIncWarn.json";
    std::string oobJsonIncFile = incFolder+"/oobIncWarn.json";
#endif
    GlobalContext() {}

    // Map global function name to function defination.
    NameFuncMap Funcs;
    localFuncMap localFuncs;
    std::map<std::string, std::set<llvm::Function*>> nameFuncs;
    std::unordered_map<const llvm::Function*, StringRef> funcToMd;
	
    // Map global object name to object definition
    GObjMap Gobjs;
    TypeNameMap GlobalTypes;

    // Map function pointers (IDs) to possible assignments
    std::set<llvm::Function*> FPtrs;
    FuncPtrMap FuncPtrs;
    // Functions whose addresses are taken.
    FuncSet AddressTakenFuncs;

    // Map a callsite to all potential callee functions.
    CalleeMap Callees;

    // Conservative mapping for callees for a callsite
    CalleeMap ConsCallees;

    //map the function to the struct which holds the function
    FuncHolder FuncHolders;
    // Map a function to all potential caller instructions.
    CallerMap Callers;

    //Map a function to the functions it calls
    CallMap CallMaps;

    CallMap DirectCallMap;
    //Map a function to the functions who call it
    CalledMap CalledMaps;

    // Map global function name to function.
	NameFuncMap GlobalFuncs;

    // Unified functions -- no redundant inline functions
	DenseMap<size_t, Function *>UnifiedFuncMap;
	std::set<Function *>UnifiedFuncSet;
    // Map function signature to functions
	DenseMap<size_t, FuncSet>sigFuncsMap;

    //Map a function to its node ptset
    //FuncNodePTSet FPtrSet;
    //FuncNodePTSet FInPtrSet;
    bool incAnalysis = false;
    //visit map used in qualifier analysis
    std::map<llvm::Function *, bool> Visit;

    //functions ready for analysis
    std::unordered_set<llvm::Function* > ReadyList;
    std::unordered_set<llvm::Function* > indFuncs;
    //calculate the # of called functions of the caller
    std::map<llvm::Function*, int> RemainedFunction;

    std::vector<std::vector<llvm::Function*>> SCC;
    std::set<llvm::Function*> rec;
    //FunctionSummary
    FtSummary FSummaries;
    FtSummary IOFSummaries;
    FtSummary OOBFSummaries;
    //The number of the uninitialized variables when declared.
    long long numDecl = 0;
    long long numUninit = 0;
    long long fieldDecl = 0;
    long long fieldUninit = 0;
    
    //The # of variables which treat as ubi with intra-procedural analysis
    //uninitArg gives the total # of variables which are uninit while passing to the callee
    long long uninitArg = 0;
    //uninitOut Arg calculate the # of variables which are not uninit if it comes from argument
    long long uninitOutArg = 0;
    //ignoreOutArg calculate the # of variables which are 
    long long ignoreOutArg = 0;

    //Function requirement
    //FuncReq FunctionReq;

    //For the finalization, change means the summary changes
    std::map<llvm::Function *, bool> ChangeMap;

    // Indirect call instructions.
    std::vector<llvm::CallInst *> IndirectCallInsts;

    // Modules.
    ModuleList Modules;
    ModuleNameMap ModuleMaps;
    std::set<std::string> InvolvedModules;
    // Some manually-summarized functions that initialize
    // values.
    std::set<std::string> HeapAllocFuncs;
    std::set<std::string> PreSumFuncs;
    std::set<std::string> CopyFuncs;
    std::set<std::string> InitFuncs;
    std::set<std::string> TransferFuncs;
    std::set<std::string> ZeroMallocFuncs;
    std::set<std::string> ObjSizeFuncs;
    std::set<std::string> StrFuncs;
    std::set<std::string> OtherFuncs;
    std::set<std::string> IndirectFuncs;
    //std::queue<std::string> printQueue;
    std::set<std::string> visitedFuncs;
    std::set<std::string> SyscallFuncs;

    // StructAnalyzer
    StructAnalyzer structAnalyzer;
    std::map<const llvm::StructType*, std::set<int>> usedField;
    //std::string heapWarning = "/data/home/yizhuo/allyes-llbc-414/heap-ind.txt";
    //std::string heapWarning = "/data/home/yizhuo/Pixel3/test.txt";
    //std::string heapWarning = "/data/home/yizhuo/test.txt";

    //used in the incremental analysis
    std::set<llvm::Function*> modifiedFuncs;
    std::unordered_map<llvm::Function*, std::set<std::pair<std::string, std::string>>> fToWarns;
    std::unordered_map<llvm::Function*, std::set<std::pair<std::string, std::string>>> fToIOWarns;
    std::unordered_map<llvm::Function*, std::set<std::pair<std::string, std::string>>> fToOOBWarns;
    std::unordered_map<llvm::Function*, bool> incCandiFuncs;
    std::unordered_map<llvm::Function*, int> incFuncRemained;
    CallMap candiCallMaps;
    CalledMap candiCalledMaps;
    ModuleList candiModules;
    std::vector<llvm::Module*> newCompiledFiles;
    std::unordered_map<llvm::Function*, int> times;
    std::unordered_set<std::string> dbgFuncs;
    std::unordered_map<llvm::Function*, double> time;

    std::unordered_map<llvm::Function*, int> numSum;
    std::unordered_map<llvm::Function*, unsigned> sumSize;

    unsigned long long ptsIns = 0;
    double ptsTime = 0.0;

    unsigned long long aaIns = 0;
    double aaTime = 0.0;

    unsigned long long inferIns = 0;
    double inferTime = 0.0;

    std::queue<llvm::Function*> tasks;
    std::unordered_map<llvm::Function*, bool> inQueue;
    
    std::unordered_map<llvm::Function*, std::set<llvm::Function*>> impact; 
	
};

class IterativeModulePass {
protected:
    GlobalContext *Ctx;
    const char * ID;
public:
    IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
            : Ctx(Ctx_), ID(ID_) { }

    // Run on each module before iterative pass.
    virtual bool doInitialization(llvm::Module *M)
    { return true; }

    // Run on each module after iterative pass.
    virtual bool doFinalization(llvm::Module *M)
    { return true; }

    // Iterative pass.
    virtual bool doModulePass(llvm::Module *M)
    { return false; }

    virtual void run(ModuleList &modules);
    virtual void collectRemaining(){}
    virtual void calDepFuncs(){}
    virtual void finalize(){}
};


#endif //UBIANALYSIS_H

