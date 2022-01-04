#include <llvm/IR/DebugInfo.h>
#include <llvm/Pass.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Analysis/CallGraph.h>
#include "llvm/IR/CallSite.h"

#include "CallGraph.h"
#include "Annotation.h"
#include "Helper.h"

//#define ADD_MLTA
#ifdef ADD_MLTA
#include "CGCommon.h"
#endif

using namespace llvm;
#define RUN_ST_ANALYZER

DenseMap<size_t, FuncSet> CallGraphPass::typeFuncsMap;
unordered_map<size_t, set<size_t>> CallGraphPass::typeConfineMap;
unordered_map<size_t, set<size_t>> CallGraphPass::typeTransitMap;
set<size_t> CallGraphPass::typeEscapeSet;
const DataLayout *CurrentLayout;
//#define PRINT_INIT
// collect function pointer assignments in global initializers
//args: Module/Iniitializer/variable/variable Id
inline size_t fHash(std::string &fName, std::string &relMName)
{
	std::hash<std::string> str_hash;
	std::string output = relMName + fName;

	return str_hash(output);
}
bool CallGraphPass::preSumFuncs(llvm::Function *F) {
        std::string fName = F->getName().str();
        if (Ctx->HeapAllocFuncs.count(fName))
                return true;
        else if (Ctx->InitFuncs.count(fName))
                return true;
        else if (Ctx->CopyFuncs.count(fName))
                return true;
        else if (Ctx->TransferFuncs.count(fName))
                return true;
        else if (Ctx->ObjSizeFuncs.count(fName))
                return true;
        else if (Ctx->PreSumFuncs.count(fName))
		return true;
	else if (F->getName().str().find("print") != std::string::npos)
		return true;
	else if (F->getName().str().find("llvm_gcov") != std::string::npos)
		return true;
	else
                return false;
}
void CallGraphPass::processInitializers(Module *M, Constant *C, GlobalValue *V, std::string Id)
{
#ifdef PRINT_INIT
	errs() << "Inside processInitializers: \n";
	if (V)
		errs() << "V: " << *V << "\n";
	if (C)
		errs() << "C: " << *C << "\n";
	if (Id.size())
		errs() << "Id: " << Id << "\n";
#endif
	// structs
	if (ConstantStruct *CS = dyn_cast<ConstantStruct>(C))
	{
		//errs()<<"struct type.\n";
		StructType *STy = CS->getType();
		if (!STy->hasName() && Id.empty())
		{
			Id = getVarId(V);
		}
		//iteratively find the function ptr inside strucrure
		for (unsigned i = 0; i != STy->getNumElements(); ++i)
		{
			Type *ETy = STy->getElementType(i);
			//consider if ETy is literal
			if (ETy->isStructTy())
			{
				const StructType *EStructTy = dyn_cast<StructType>(ETy);
				if (STy->isLiteral() || EStructTy->isLiteral())
					continue;
				std::string new_id;
				if (Id.empty())
					new_id = STy->getStructName().str() + "," + Twine(i).str();
				else
					new_id = Id + "," + Twine(i).str();
				//errs()<<"new_id = "<<new_id<<"\n";
				processInitializers(M, CS->getOperand(i), NULL, new_id);
			}
			else if (ETy->isArrayTy())
			{
				// nested array of struct
				processInitializers(M, CS->getOperand(i), NULL, "");
			}
			else if (isFunctionPointer(ETy))
			{
				//OP<<"Element "<<i<<" is a Function Ptr.\n";
				// found function pointers in struct fields
				if (Function *F = dyn_cast<Function>(CS->getOperand(i)))
				{
					std::string new_id;
					if (!STy->isLiteral())
					{
						if (STy->getStructName().startswith("struct.anon.") ||
							STy->getStructName().startswith("union.anon"))
						{
							if (Id.empty())
								new_id = getStructId(STy, M, i);
						}
						else
						{
							new_id = getStructId(STy, M, i);
						}
					}
					else
					{
						//	OP<<"isLiteral. in module "<<F->getParent()->getName().str()<<"\n";
					}
					if (new_id.empty())
					{
						if (!Id.size() && STy->isLiteral())
							continue;
						assert(Id.size() > 0);
						new_id = Id + "," + Twine(i).str();
					}
					Ctx->FuncPtrs[new_id].insert(F);
					//OP<<"i = "<<i<<"\n";
					//OP<<"new_id "<<new_id<<"insert "<<F->getName().str()<<"\n";
				}
			}
		}
	}
	else if (ConstantArray *CA = dyn_cast<ConstantArray>(C))
	{
		//OP<<"Const Array : "<<*CA<<"\n";
		//OP<<"numOperands() = "<<CA->getNumOperands()<<"\n";
		// array, conservatively collects all possible pointers
		for (unsigned i = 0; i != CA->getNumOperands(); ++i)
		{
			//OP<<"i = "<<i<<"\n";
			processInitializers(M, CA->getOperand(i), V, Id);
		}
	}
	else if (Function *F = dyn_cast<Function>(C))
	{
		// global function pointer variables
		if (V)
		{
			std::string Id = getVarId(V);
			//OP<<"getVarId ="<<Id<<"\n";
			Ctx->FuncPtrs[Id].insert(F);
		}
	}
}

bool CallGraphPass::mergeFuncSet(FuncSet &S, const std::string &Id, bool InsertEmpty)
{
	FuncPtrMap::iterator i = Ctx->FuncPtrs.find(Id);
	if (i != Ctx->FuncPtrs.end()){
	    return mergeFuncSet(S, i->second);
	    errs()<<"Ctx->FuncPtrs.insert("<<Id<<"):\n";
            for (auto item : i->second) {
		errs()<<item->getName().str()<<"/";	
	    }
	    errs()<<"\n";
	}
	else if (InsertEmpty){
		Ctx->FuncPtrs.insert(std::make_pair(Id, FuncSet()));
	}
	return false;
}

bool CallGraphPass::mergeFuncSet(std::string &Id, const FuncSet &S, bool InsertEmpty)
{
	FuncPtrMap::iterator i = Ctx->FuncPtrs.find(Id);
	if (i != Ctx->FuncPtrs.end())
	{
#ifdef PRINT_MERGE
		OP << "find functions.\n";
		OP << "Before merge: \n";
		for (auto item : Ctx->FuncPtrs[Id])
		{
			OP << item->getName().str() << "/";
		}
		OP << "\n";
#endif
		bool ret = mergeFuncSet(i->second, S);
#ifdef PRINT_MERGE
		OP << "After merge: \n";
		for (auto item : Ctx->FuncPtrs[Id])
		{
			OP << item->getName().str() << "/";
		}
		OP << "\n";
#endif
		return ret;
	}
	else if (!S.empty())
	{
#ifdef PRINT_MERGE
		OP << "function not empty.\n";
		OP << "Before merge: \n";
		for (auto item : Ctx->FuncPtrs[Id])
		{
			OP << item->getName().str() << "/";
		}
		OP << "\n";
#endif
		bool ret = mergeFuncSet(Ctx->FuncPtrs[Id], S);
#ifdef PRINT_MERGE
		OP << "After merge: \n";
		for (auto item : Ctx->FuncPtrs[Id])
		{
			OP << item->getName().str() << "/";
		}
		OP << "\n";
#endif
		return ret;
	}
	else if (InsertEmpty)
	{
		//OP << "insertEmpty == NULL.\n";
		Ctx->FuncPtrs.insert(std::make_pair(Id, FuncSet()));
	}
	return false;
}
void CallGraphPass::findCalleesByType(CallInst *CI, FuncSet &S) {
    //errs()<<"find Callee for "<<*CI<<" in function "<<CI->getParent()->getParent()->getName().str();
    if (CI->isInlineAsm())
        return;

    CallSite CS(CI);
    for (Function *F : Ctx->AddressTakenFuncs) {
    //for (Function *F : Ctx->FPtrs) {
        // VarArg
        if (F->getFunctionType()->isVarArg()) {
            // Compare only known args in VarArg.
            continue;
        }
        // otherwise, the numbers of args should be equal.
        else if (F->arg_size() != CS.arg_size()) {
            continue;
        }
	#ifdef STORE
	else if (CI->getNumArgOperands() == 0 && F->getReturnType()->isVoidTy()) {
	    continue;
	}
	#endif
	
        //Type matching on args.
        bool Matched = true;
        CallSite::arg_iterator AI = CS.arg_begin();
	int argNo = 0;
        for (Function::arg_iterator FI = F->arg_begin(), FE = F->arg_end();
             FI != FE; ++FI, ++AI) {
            // check type mis-match
	    //FormalTy comes from candidate function
            Type *FormalTy = FI->getType();
	    //ActualTy comes from call Instruction/caller
            Type *ActualTy = (*AI)->getType();
	    
            if (isCompatibleType(FormalTy, ActualTy, F, argNo)){
            	argNo++;
	        continue;
	    }
            else {
                Matched = false;
                break;
            }
	    argNo++;
        }	
        if (Matched) {
            S.insert(F);
        }
    }
}
bool CallGraphPass::mergeFuncSet(FuncSet &Dst, const FuncSet &Src)
{
	bool Changed = false;
	for (FuncSet::const_iterator i = Src.begin(), e = Src.end(); i != e; ++i)
	{
		assert(*i);
		Changed |= Dst.insert(*i).second;
	}
	return Changed;
}

void CallGraphPass::findFunctionsCons(CallInst *CI, Module *M, FuncSet &S)
{
	if (CI->isInlineAsm())
		return;
	//errs()<<"Inside findFunctionsCons: \n";
	CallSite CS(CI);
	//errs() << *CI << "\n";
	for (Function &F : *M)
	{
		if (!F.hasAddressTaken())
		{
			//errs() << "Not addr taken: " << F.getName() << "\n";
			continue;
		}

		// FIXME unlikely to have indirect call to vararg functions?
		if (F.getFunctionType()->isVarArg())
		{
			//errs() << "VarArg: " << F.getName() << "\n";
			continue;
		}

		if (F.arg_size() != CS.arg_size())
		{
			//errs() << "ArgNum mismatch: " << F.getName() << "\n";
			continue;
		}

		if (F.isIntrinsic())
		{
			//errs() << "Intrinsic: " << F.getName() << "\n";
			continue;
		}

		CallSite::arg_iterator AI = CS.arg_begin();
		for (Function::arg_iterator FI = F.arg_begin(), FE = F.arg_end();
			 FI != FE; ++FI, ++AI)
		{
			// check type mis-match
			Argument *formal = FI;
			Value *actual = *AI;

			if (formal->getType() != actual->getType())
			{
				//WARNING("ArgType mismatch: " << F.getName()
				//		<< ", arg " << FI->getArgNo() << "\n");
				break;
			}
		}

		if (AI != CS.arg_end())
			continue;

		//errs() << "Insert: " << F.getName() << "\n";
		S.insert(&F);
	}
}

bool CallGraphPass::findFunctions(Value *V, FuncSet &S)
{
	SmallPtrSet<Value *, 4> Visited;
	return findFunctions(V, S, Visited);
}

bool CallGraphPass::findFunctions(Value *V, FuncSet &S,
				  SmallPtrSet<Value *, 4> Visited)
{
	//OP<<"Inside findFunctions:\n";
	//OP<<"V : "<<*V<<"\n";
	if (!Visited.insert(V).second)
		return false;
	// real function, S = S + {F}
	if (Function *F = dyn_cast<Function>(V))
	{
		//if (!F->empty())
		//	return S.insert(F).second;

		// prefer the real definition to declarations
		if (F->empty())
		{
			NameFuncMap::iterator it = Ctx->Funcs.find(F->getName().str());
			if (it != Ctx->Funcs.end())
				F = it->second;
		}
		return S.insert(F).second;
	}
	// bitcast, ignore the cast
	if (CastInst *B = dyn_cast<CastInst>(V))
		return findFunctions(B->getOperand(0), S, Visited);

	// const bitcast, ignore the cast
	if (ConstantExpr *C = dyn_cast<ConstantExpr>(V))
	{
		if (C->isCast())
		{
		    return findFunctions(C->getOperand(0), S, Visited);
		}
		// FIXME GEP
	}
	if (GetElementPtrInst *G = dyn_cast<GetElementPtrInst>(V))
	{
		return false;
	}
	else if (isa<ExtractValueInst>(V))
	{
		return false;
	}
	if (isa<AllocaInst>(V))
	{
		return false;
	}
	if (BinaryOperator *BO = dyn_cast<BinaryOperator>(V))
	{
		Value *op0 = BO->getOperand(0);
		Value *op1 = BO->getOperand(1);
		if (!isa<Constant>(op0) && isa<Constant>(op1))
			return findFunctions(op0, S, Visited);
		else if (isa<Constant>(op0) && !isa<Constant>(op1))
			return findFunctions(op1, S, Visited);
		else
			return false;
	}
	// PHI node, recursively collect all incoming values
	if (PHINode *P = dyn_cast<PHINode>(V))
	{
		bool Changed = false;
		for (unsigned i = 0; i != P->getNumIncomingValues(); ++i)
			Changed |= findFunctions(P->getIncomingValue(i), S, Visited);
		return Changed;
	}

	// select, recursively collect both paths
	if (SelectInst *SI = dyn_cast<SelectInst>(V))
	{
		bool Changed = false;
		Changed |= findFunctions(SI->getTrueValue(), S, Visited);
		Changed |= findFunctions(SI->getFalseValue(), S, Visited);
		return Changed;
	}
	// arguement, S = S + FuncPtrs[arg.ID]
	if (Argument *A = dyn_cast<Argument>(V))
	{
		bool InsertEmpty = isFunctionPointer(A->getType());
		return mergeFuncSet(S, getArgId(A), InsertEmpty);
	}

	// return value, S = S + FuncPtrs[ret.ID]
	if (CallInst *CI = dyn_cast<CallInst>(V))
	{
		// update callsite info first
		FuncSet &FS = Ctx->Callees[CI];
		//FS.setCallerInfo(CI, &Ctx->Callers);
		findFunctions(CI->getCalledValue(), FS);
		bool Changed = false;
		for (Function *CF : FS)
		{
			bool InsertEmpty = isFunctionPointer(CI->getType());
			Changed |= mergeFuncSet(S, getRetId(CF), InsertEmpty);
		}
		return Changed;
	}
	// loads, S = S + FuncPtrs[struct.ID]
	if (LoadInst *L = dyn_cast<LoadInst>(V))
	{
		std::string Id = getLoadId(L);

		if (!Id.empty())
		{
			bool InsertEmpty = isFunctionPointer(L->getType());
			return mergeFuncSet(S, Id, InsertEmpty);
		}
		else
		{
			Function *f = L->getParent()->getParent();
			//errs() << "Empty LoadID: " << f->getName() << "::" << *L << "\n";
			return false;
		}
	}
	// ignore other constant (usually null), inline asm and inttoptr
	if (isa<Constant>(V) || isa<InlineAsm>(V) || isa<IntToPtrInst>(V))
		return false;
	//V->dump();
	//report_fatal_error("findFunctions: unhandled value type\n");
	errs() << "findFunctions: unhandled value type: " << *V << "\n";
	return false;
}

bool CallGraphPass::runOnFunction(Function *F)
{
	bool Changed = false;
	if (F->isIntrinsic()) {
	    return Changed;
	}
	//OP<<"run on function F: "<<F->getName().str()<<"\n";

	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i)
	{
		Instruction *I = &*i;
		//OP<<*I<<"\n";
		if (StoreInst *SI = dyn_cast<StoreInst>(I))
		{
			// stores to function pointers
			Value *V = SI->getValueOperand();

			if (isFunctionPointerOrVoid(V->getType()))
			{
				//OP<<"Store Ins: "<<*I<<"\n";
				std::string Id = getStoreId(SI);
				//OP<<"StoreId : "<<Id<<"\n";
				if (!Id.empty())
				{
					FuncSet FS;
					findFunctions(V, FS);
					Changed |= mergeFuncSet(Id, FS, isFunctionPointer(V->getType()));
				}
				else
				{
					//errs() << "Empty StoreID: " << F->getName() << "::" << *SI << "\n";
				}
			}
		}
		else if (ReturnInst *RI = dyn_cast<ReturnInst>(I))
		{
			// function returns
			if (isFunctionPointerOrVoid(F->getReturnType()))
			{
				Value *V = RI->getReturnValue();
				std::string Id = getRetId(F);
				//OP<<"retId : "<<Id<<"\n";
				FuncSet FS;
				findFunctions(V, FS);

				Changed |= mergeFuncSet(Id, FS, isFunctionPointer(V->getType()));
			}
		}
		else if (CallInst *CI = dyn_cast<CallInst>(I))
		{
			// ignore inline asm or intrinsic calls
			if (CI->isInlineAsm() || isa<DbgInfoIntrinsic>(I))
			{
#if 0
				InlineAsm *Asm = dyn_cast<InlineAsm>(CI->getCalledValue());
				errs() << "InlineAsm: " << F->getParent()->getModuleIdentifier() << "::"
					   << F->getName().str() << "::" << Asm->getAsmString() << "\n";
#endif
				continue;
			}
#ifdef CALLMAP
			if (Function *CF = CI->getCalledFunction())
			{
				if (CF->isIntrinsic() || CF->isVarArg())
					continue;
				//store the direct call to CallMaps and CalledMaps
				if (CF->empty())
				{
					NameFuncMap::iterator it = Ctx->Funcs.find(CF->getName().str());
					if (it != Ctx->Funcs.end())
						CF = it->second;
				}
				if (CF->empty())
					continue;
				if (!preSumFuncs(CF)) {
                        	    Ctx->CallMaps[F].insert(CF);
				    Ctx->CalledMaps[CF].insert(F);
				}	
			}
#endif
			CallSite CS(CI);
			// might be an indirect call, find all possible callees
			FuncSet &FS = Ctx->Callees[CI];
			//FS.setCallerInfo(CI, &Ctx->Callers);
			//OP<<"CI->getCalledValue: "<<*CI->getCalledValue()<<"\n";
			//errs()<<"1findFunctions \n";
			#ifdef ADD_MLTA
			if (CS.isIndirectCall()) {
			    findCalleesWithMLTA(CI, FS);
			    //findCalleesByType(CI,FS);
			}
			#endif
			if (!findFunctions(CI->getCalledValue(), FS) && FS.empty())
				continue;
			//errs()<<"1end.\n";
			for (Function *Callee : FS)
			{
				if (Callee->isIntrinsic() || Callee->isVarArg())
					continue;
				Function *CF = Callee;
				if (Callee->empty())
				{
					NameFuncMap::iterator it = Ctx->Funcs.find(CF->getName().str());
					if (it != Ctx->Funcs.end())
						CF = it->second;
				}
				if (CF->empty())
					continue;
				if (preSumFuncs(CF)) {
                         	       continue;
                        	}
				Ctx->CallMaps[F].insert(CF);
				Ctx->CalledMaps[CF].insert(F);
			}
			// looking for function pointer arguments
			for (unsigned no = 0, ne = CI->getNumArgOperands(); no != ne; ++no)
			{
				Value *V = CI->getArgOperand(no);
				if (!isFunctionPointerOrVoid(V->getType()))
					continue;
				// find all possible assignments to the argument
				FuncSet VS;
				if (!findFunctions(V, VS))
					continue;
				// update argument FP-set for possible callees
				for (Function *CF : FS)
				{
					if (!CF)
					{
						errs() << "NULL Function " << *CI << "\n";
						assert(0);
					}
					std::string Id = getArgId(CF, no);
					//OP<<"ArgId : "<<Id<<"\n";
					Changed |= mergeFuncSet(Ctx->FuncPtrs[Id], VS);
				}
			}
		}
	}
	return Changed;
}

bool CallGraphPass::doInitialization(Module *M)
{
	const DataLayout *DL = &(M->getDataLayout());
	CurrentLayout = DL;
        Int8PtrTy = Type::getInt8PtrTy(M->getContext());
        IntPtrTy = DL->getIntPtrType(M->getContext());
	StringRef MName = Ctx->ModuleMaps[M];
// get struct info
#ifdef RUN_ST_ANALYZER
	//OP<<"run st analyzer for "<<Ctx->ModuleMaps[M]<<": "<<M<<"\n";
	Ctx->structAnalyzer.run(M, DL);
#endif

	// collect function pointer assignments in global initializers
	for (GlobalVariable &g : M->globals())
	{
		//if the global variable is initialized (not declaration)
		if (g.hasInitializer())
		{
			//OP<<"=============\n";
			//collect the map from global name to it's function : Ctx->FuncPtrs
			processInitializers(M, g.getInitializer(), &g, "");

			if (g.hasExternalLinkage())
				Ctx->Gobjs[g.getName()] = &g;
		}
	}


	//Used for MLTA
	#ifdef ADD_MLTA
	for (Module::global_iterator gi = M->global_begin();
                        gi != M->global_end(); ++gi) {
                GlobalVariable* GV = &*gi;
                if (!GV->hasInitializer())
                        continue;
                Constant *Ini = GV->getInitializer();
                if (!isa<ConstantAggregate>(Ini))
                        continue;

                typeConfineInInitializer(Ini);
        }
	#endif
	// collect global function definitions
	for (Function &f : *M)
	{
		if (f.isIntrinsic())
			continue;
		if (!f.empty()) {
		    //OP<<"funcToMd ["<<&f<<"] = "<<MName<<"\n";
		    Ctx->funcToMd[&f] = MName;
		}
		if (preSumFuncs(&f)) {
			Ctx->dbgFuncs.insert(f.getName().str());
		}
		#ifdef ADD_MLTA
		if (Ctx->funcToMd[&f].find("driver")!=std::string::npos || (Ctx->funcToMd[&f].find("fs")!=std::string::npos && Ctx->funcToMd[&f].find("dev")!=std::string::npos)) {
		    
		   if (f.isDeclaration())
                        continue;

                    for (inst_iterator i = inst_begin(f), e = inst_end(f);
                                i != e; ++i) {
                        Instruction *I = &*i;

                        if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
                                typeConfineInStore(SI);
                        }
                        else if (CastInst *CastI = dyn_cast<CastInst>(I)) {
                                typeConfineInCast(CastI);
                        }
                    }


		    if (f.hasAddressTaken()) {
			Ctx->AddressTakenFuncs.insert(&f);
			Ctx->sigFuncsMap[funcHash(&f, false)].insert(&f);
		    }
		}
		#endif
		if (f.hasExternalLinkage() && !f.empty()){
			// external linkage always ends up with the function name
			//OP<<"Ctx->Funcs.inset: ("<<f.getName().str()<<")"<<"\n";
			Ctx->Funcs[f.getName().str()] = &f;
			Ctx->nameFuncs[f.getName().str()].insert(&f);
			//OP<<"size of nameFuncs = "<<Ctx->nameFuncs.size()<<"\n";
		}
		else
		{
			std::string fName = f.getName().str();
			std::string mName = Ctx->ModuleMaps[M];//M->getName().str();
			//OP<<"mName = "<<mName<<"\n";
			size_t pos = mName.find("lll-");
			std::string relMName = "";
			bool slash = false;

			if (pos == std::string::npos)
			{
				continue;
			}
			pos = mName.find_first_of("/", pos + 1);
			/*reletive path of the file.*/
			relMName += mName.substr(pos + 1);

			//OP<<"Insert hash for "<<fName<<", "<<relMName<<"\n";
			Ctx->localFuncs[fHash(fName, relMName)] = &f;
			/*
		for (; pos < mName.size(); pos++) {
       		    if (!slash && mName[pos] == "/") {
           		slash = true;
       		    }
       		    else if (slash) {
               		relMName += mName[pos];
       		    }
   		}*/
			//OP<<"relMName: "<<relMName<<", fname = "<<fName<<"\n";
			//OP<<"Hash :"<<fHash(fName, relMName)<<" : "<<&f<<"\n";
		}
	}

	return false;
}

bool CallGraphPass::doFinalization(Module *M)
{
	// update callee mapping
	for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f)
	{
		Function *F = &*f;
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i)
		{
			// map callsite to possible callees
			if (CallInst *CI = dyn_cast<CallInst>(&*i))
			{
				FuncSet &FS = Ctx->Callees[CI];
				//FS.setCallerInfo(CI, &Ctx->Callers);
				findFunctions(CI->getCalledValue(), FS);

				// if the target is empty, we still need to report
				// this code pointer
#if 0
				if (FS.empty()) {
					outs() << getAnnotation(CI->getCalledValue(), M) << "\n";
				}
#endif

				// calculate the caller info here
				for (Function *CF : FS)
				{
					CallInstSet &CIS = Ctx->Callers[CF];
					CIS.insert(CI);
					/*if (CF->isIntrinsic())
						continue;	
					Function *Callee = CF;
					if (Callee->empty()) {
					    NameFuncMap::iterator it = Ctx->Funcs.find(getScopeName(Callee));
                                	    if (it != Ctx->Funcs.end())
                                        	Callee = it->second;
					}
					if (Callee->empty())
					    continue;
					Ctx->CallMaps[F].insert(Callee);
					Ctx->CalledMaps[Callee].insert(F);*/
				}

				if (!CI->getCalledFunction())
				{
					FuncSet &CFS = Ctx->ConsCallees[CI];
					findFunctionsCons(CI, M, CFS);

					/*for (Function *CF : FS) {
					    if (CF->isIntrinsic())
                                                continue;
					    Function *Callee = CF;
					    if (Callee->empty()) {
					        NameFuncMap::iterator it = Ctx->Funcs.find(getScopeName(Callee));
                                	        if (it != Ctx->Funcs.end())
                                        	    Callee = it->second;
					    }
					    if (Callee->empty())
						continue;
                                            Ctx->CallMaps[F].insert(Callee);
                                            Ctx->CalledMaps[Callee].insert(F);
                                	}*/
				}
			}
		}
	}
	return false;
}

bool CallGraphPass::doModulePass(Module *M)
{
	bool Changed = true, ret = false;
	while (Changed)
	{
		Changed = false;
		for (Module::iterator i = M->begin(), e = M->end(); i != e; ++i)
			Changed |= runOnFunction(&*i);
		ret |= Changed;
	}
	return ret;
}

// debug
void CallGraphPass::dumpFuncPtrs()
{
	raw_ostream &OS = outs();
	for (FuncPtrMap::iterator i = Ctx->FuncPtrs.begin(),
							  e = Ctx->FuncPtrs.end();
		 i != e; ++i)
	{
		//if (i->second.empty())
		//	continue;
		OS << i->first << "\n";
		FuncSet &v = i->second;
		for (FuncSet::iterator j = v.begin(), ej = v.end();
			 j != ej; ++j)
		{
			OS << "  " << ((*j)->hasInternalLinkage() ? "f" : "F")
			   << " " << (*j)->getName().str() << "\n";
		}
	}
}

void CallGraphPass::fillCallGraphs()
{
	for (CalleeMap::iterator i = Ctx->Callees.begin(),
							 e = Ctx->Callees.end();
		 i != e; ++i)
	{

		CallInst *CI = i->first;
		llvm::Function *F = CI->getParent()->getParent();

		FuncSet &v = i->second;
		if (CI->isInlineAsm() /*|| v.empty()*/)
			continue;
		if (CI->getCalledFunction())
		{
			llvm::Function *CF = CI->getCalledFunction();
			if (CF->isIntrinsic())
				continue;
			std::string fName = CF->getName().str();
			if (CF->empty())
			{
				NameFuncMap::iterator it = Ctx->Funcs.find(fName);
				if (it != Ctx->Funcs.end())
					CF = it->second;
			}
			if (CF->empty())
				continue;
			if (!preSumFuncs(CF)) {
			    Ctx->CallMaps[F].insert(CF);
			    Ctx->CalledMaps[CF].insert(F);
                        }
		}

		//const DebugLoc &LOC = CI->getDebugLoc();
		//OS << "LOC: ";
		//LOC.print(OS);
		//OS << "^@^";
		//if (v.empty())
		//        errs() << "!!EMPTY =>" << *CI->getCalledValue();
		for (FuncSet::iterator j = v.begin(), ej = v.end();
			 j != ej; ++j)
		{
			//OS << "\t" << ((*j)->hasInternalLinkage() ? "f" : "F")
			//      << " " << (*j)->getName() << "\n";
			llvm::Function *CF = *j;
			std::string fName = CF->getName().str();
			if (CF->empty())
			{
				NameFuncMap::iterator it = Ctx->Funcs.find(fName);
				if (it != Ctx->Funcs.end())
					CF = it->second;
			}
			if (CF->empty())
				continue;
			if (preSumFuncs(CF)) {
                                continue;
                        }
			Ctx->CallMaps[F].insert(CF);
			Ctx->CalledMaps[CF].insert(F);
			//OS << (*j)->getName() << "::";
		}
		//OS << "\n";

		v = Ctx->ConsCallees[CI];
		for (FuncSet::iterator j = v.begin(), ej = v.end();
			 j != ej; ++j)
		{
			llvm::Function *CF = *j;
			if (CF->isIntrinsic())
				continue;

			std::string fName = CF->getName().str();
			if (CF->empty())
			{
				NameFuncMap::iterator it = Ctx->Funcs.find(fName);
				if (it != Ctx->Funcs.end())
					CF = it->second;
			}
			if (CF->empty())
				continue;
			if (preSumFuncs(CF)) {
                                continue;
                        }
			Ctx->CallMaps[F].insert(CF);
			Ctx->CalledMaps[CF].insert(F);
			//OS << (*j)->getName() << "::";
		}
		//OS << "\n";
	}
	//errs()<<"\n[End of dumpCallees]\n";
}

void CallGraphPass::dumpCallees()
{
	errs() << "\n[dumpCallees]\n";
	raw_ostream &OS = outs();
	OS << "Num of Callees: " << Ctx->Callees.size() << "\n";
	for (CalleeMap::iterator i = Ctx->Callees.begin(),
							 e = Ctx->Callees.end();
		 i != e; ++i)
	{

		CallInst *CI = i->first;
		FuncSet &v = i->second;
		if (CI->isInlineAsm() || CI->getCalledFunction() /*|| v.empty()*/)
			continue;

		OS << "CS:" << *CI << "\n";
		const DebugLoc &LOC = CI->getDebugLoc();
		OS << "LOC: ";
		LOC.print(OS);
		OS << "^@^";
		if (v.empty())
			OS << "!!EMPTY =>" << *CI->getCalledValue();
		for (FuncSet::iterator j = v.begin(), ej = v.end();
			 j != ej; ++j)
		{
			//OS << "\t" << ((*j)->hasInternalLinkage() ? "f" : "F")
			//	<< " " << (*j)->getName() << "\n";
			OS << (*j)->getName() << "::";
		}
		OS << "\n";

		v = Ctx->ConsCallees[CI];
		OS << "ConsCallees: ";
		for (FuncSet::iterator j = v.begin(), ej = v.end();
			 j != ej; ++j)
		{
			OS << (*j)->getName() << "::";
		}
		OS << "\n";
	}
	errs() << "\n[End of dumpCallees]\n";
}

void CallGraphPass::dumpCallers()
{
	errs() << "\n[dumpCallers]\n";
	for (auto M : Ctx->Callers)
	{
		Function *F = M.first;
		CallInstSet &CIS = M.second;
		errs() << "F : " << F->getName() << "\n";

		for (CallInst *CI : CIS)
		{
			Function *CallerF = CI->getParent()->getParent();
			errs() << "\t";
			if (CallerF && CallerF->hasName())
			{
				errs() << "(" << CallerF->getName() << ") ";
			}
			else
			{
				errs() << "(anonymous) ";
			}

			errs() << *CI << "\n";
		}
	}
	errs() << "\n[End of dumpCallers]\n";
}
