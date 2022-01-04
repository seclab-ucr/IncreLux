#ifndef _CALL_GRAPH_H
#define _CALL_GRAPH_H

#include "UBIAnalysis.h"

using namespace std;
using namespace llvm;
class CallGraphPass : public IterativeModulePass {
private:
	const DataLayout *DL;
        // char * or void *
        Type *Int8PtrTy;
        // long interger type
        Type *IntPtrTy;
	
	bool runOnFunction(llvm::Function*);
	void findCalleesByType(llvm::CallInst*, FuncSet&);
	void processInitializers(llvm::Module*, llvm::Constant*, llvm::GlobalValue*, std::string);
	bool mergeFuncSet(FuncSet &S, const std::string &Id, bool InsertEmpty);
	bool mergeFuncSet(std::string &Id, const FuncSet &S, bool InsertEmpty);
	bool mergeFuncSet(FuncSet &Dst, const FuncSet &Src);
	void findFunctionsCons(llvm::CallInst*, llvm::Module*, FuncSet&);
	bool findFunctions(llvm::Value*, FuncSet&);
	bool findFunctions(llvm::Value*, FuncSet&, 
	                   llvm::SmallPtrSet<llvm::Value*,4>);


	static DenseMap<size_t, FuncSet>typeFuncsMap;
        static unordered_map<size_t, std::set<size_t>>typeConfineMap;
        static unordered_map<size_t, std::set<size_t>>typeTransitMap;
        static set<size_t>typeEscapeSet;

                // Use type-based analysis to find targets of indirect calls
                void findCalleesWithType(llvm::CallInst*, FuncSet&);

                void unrollLoops(Function *F);

                bool isCompositeType(Type *Ty);
                bool typeConfineInInitializer(User *Ini);
                bool typeConfineInStore(StoreInst *SI);
                bool typeConfineInCast(CastInst *CastI);
                void escapeType(Type *Ty, int Idx = -1);
                void transitType(Type *ToTy, Type *FromTy,
                                int ToIdx = -1, int FromIdx = -1);

                Value *nextLayerBaseType(Value *V, Type * &BTy, int &Idx,
                                const DataLayout *DL);

                void funcSetIntersection(FuncSet &FS1, FuncSet &FS2,
                                FuncSet &FS);
                bool findCalleesWithMLTA(CallInst *CI, FuncSet &FS);





public:
	CallGraphPass(GlobalContext *Ctx_)
		: IterativeModulePass(Ctx_, "CallGraph") { }
	virtual bool doInitialization(llvm::Module *);
	virtual bool doFinalization(llvm::Module *);
	virtual bool doModulePass(llvm::Module *);

	// debug
	void dumpFuncPtrs();
	void dumpCallees();
	void dumpCallers();
	void fillCallGraphs();
	bool preSumFuncs(llvm::Function *F);
};

#endif
