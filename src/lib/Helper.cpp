#include <llvm/IR/Operator.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm/IR/InstIterator.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/BasicBlock.h>
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include <utility>
#include <list>
#include <unordered_set>
#include <queue>

#include "Helper.h"

using namespace llvm;
std::string StrFN[] = {
    "strcmp",
    "strncmp",
    "strcpy",
    "strlwr",
    "strcat",
    "strlen",
    "strupr",
    "strrchr",
    "strncat",
    "strsep",
    "snprintf",
    "vscnprintf",
    "vsnprintf",
    "",
};
std::set<std::string> StrFuncs(StrFN, StrFN + 9);

bool getRealType(llvm::Value *V, llvm::CallInst *CI, Type *T1, Type *T2, std::set<llvm::Function *> &);
bool checkCastType(Type *T1, Type *T2, llvm::Function *F, int argNo, std::set<llvm::Function *> &);
int64_t getGEPOffset(const Value *value, const DataLayout *dataLayout)
{
    // Assume this function always receives GEP value
    //errs()<<"Inside getGEPOffset:\n";
    const GEPOperator *gepValue = dyn_cast<GEPOperator>(value);
    assert(gepValue != NULL && "getGEPOffset receives a non-gep value!");
    assert(dataLayout != nullptr && "getGEPOffset receives a NULL dataLayout!");

    int64_t offset = 0;
    const Value *baseValue = gepValue->getPointerOperand()->stripPointerCasts();
    // If we have yet another nested GEP const expr, accumulate its offset
    // The reason why we don't accumulate nested GEP instruction's offset is
    // that we aren't required to. Also, it is impossible to do that because
    // we are not sure if the indices of a GEP instruction contains all-consts or not.
    if (const ConstantExpr *cexp = dyn_cast<ConstantExpr>(baseValue))
        if (cexp->getOpcode() == Instruction::GetElementPtr)
        {
            offset += getGEPOffset(cexp, dataLayout);
        }
    Type *ptrTy = gepValue->getSourceElementType();

    SmallVector<Value *, 4> indexOps(gepValue->op_begin() + 1, gepValue->op_end());
    // Make sure all indices are constants
    for (unsigned i = 0, e = indexOps.size(); i != e; ++i)
    {
        if (!isa<ConstantInt>(indexOps[i]))
            indexOps[i] = ConstantInt::get(Type::getInt32Ty(ptrTy->getContext()), 0);
    }
    offset += dataLayout->getIndexedOffsetInType(ptrTy, indexOps);
    return offset;
}

unsigned offsetToFieldNum(const Value *ptr, int64_t offset, const DataLayout *dataLayout,
                          const StructAnalyzer *structAnalyzer, Module *module)
{
    assert(ptr->getType()->isPointerTy() && "Passing a non-ptr to offsetToFieldNum!");
    assert(dataLayout != nullptr && "DataLayout is NULL when calling offsetToFieldNum!");
    if (offset < 0)
        return 0;
    //errs()<<"1\n";
    Type *trueElemType = cast<PointerType>(ptr->getType())->getElementType();
    //errs()<<"Inside offset to field num:\n";
    //errs()<<"1trueElemType: "<<*trueElemType<<"\n";
    unsigned ret = 0;
    if (trueElemType->isStructTy())
    {
        StructType *stType = cast<StructType>(trueElemType);
        if (!stType->isLiteral() && stType->getName().startswith("union"))
            return ret;
        if (!stType->isLiteral() && stType->getName().startswith("union"))
            return ret;
        if (stType->isOpaque())
            return ret;
    }
    //errs()<<"2\n";
    //errs()<<"offset = "<<offset<<"\n";
    while (offset > 0)
    {
        //errs()<<"offset: "<<offset<<"\n";
        // Collapse array type
        while (const ArrayType *arrayType = dyn_cast<ArrayType>(trueElemType))
            trueElemType = arrayType->getElementType();

        if (trueElemType->isStructTy())
        {
            StructType *stType = cast<StructType>(trueElemType);

            //errs()<<"2ndType: "<<*stType<<"\n";
            const StructInfo *stInfo = structAnalyzer->getStructInfo(stType, module);
            assert(stInfo != NULL && "structInfoMap should have info for all structs!");
            //if (!stInfo) return 0;
	    stType = const_cast<StructType *>(stInfo->getRealType());

            //errs()<<"2ndType: "<<*stType<<"\n";
            const StructLayout *stLayout = stInfo->getDataLayout()->getStructLayout(stType);
            //errs()<<"stInfo->getDataLayout()->getTypeAllocSize(stType) = "<< stInfo->getDataLayout()->getTypeAllocSize(stType)<<"\n";
            uint64_t allocSize = stInfo->getDataLayout()->getTypeAllocSize(stType);
            if (!allocSize)
                return 0;
            offset %= allocSize;
            unsigned idx = stLayout->getElementContainingOffset(offset);
            if (!stType->isLiteral() && stType->getName().startswith("union"))
            {
                //errs()<<"uniontype.\n";
                offset -= stInfo->getDataLayout()->getTypeAllocSize(stType);
                //errs()<<"offset = "<<offset<<"\n";
                if (offset <= 0)
                    break;
            }
            ret += stInfo->getOffset(idx);
            //errs()<<"ret = "<<ret<<"\n";

            if (!stType->isLiteral() && stType->getName().startswith("union"))
            {
                offset -= stInfo->getDataLayout()->getTypeAllocSize(stType);
            }
            else
            {
                offset -= stLayout->getElementOffset(idx);
            }
            trueElemType = stType->getElementType(idx);
        }
        else
        {
            //errs()<<"trueElemType: "<<*trueElemType<<"\n";
            //  errs()<<"dataLayout->getTypeAllocSize(trueElemType) = "<<dataLayout->getTypeAllocSize(trueElemType)<<"\n";
            offset %= dataLayout->getTypeAllocSize(trueElemType);
            if (offset != 0)
            {
                errs() << "Warning: GEP into the middle of a field. This usually occurs when union is used. Since partial alias is not supported, correctness is not guanranteed here.\n";
                break;
            }
        }
    }
    return ret;
}

std::vector<std::string> strSplit(std::string &str, const std::string pattern)
{
    std::vector<std::string> ret;
    if (str == "")
        return ret;
    std::string strs = str + pattern;
    size_t pos = strs.find(pattern);
    while (pos != strs.npos)
    {
        std::string temp = strs.substr(0, pos);
        ret.push_back(temp);
        strs = strs.substr(pos + 1, strs.size());
        pos = strs.find(pattern);
    }
    return ret;
}

#define VAR_MATCH_VAR (false)
bool isCompatibleType(Type *T1, Type *T2, llvm::Function *func, int argNo)
{
    if (T1->isPointerTy())
    {
        if (!T2->isPointerTy())
            return false;

        Type *ElT1 = T1->getPointerElementType();
        Type *ElT2 = T2->getPointerElementType();

        // assume "void *" and "char *" are equivalent to any pointer type
        if (ElT1->isIntegerTy(8) /*|| ElT2->isIntegerTy(8)*/)
        {
            if (func && func->hasName())
                errs() << "Candidate function :" << func->getName().str() << "\n";
            //errs()<<"argNo = "<<argNo<<", inside isCompatibleType: \n";
            //if (const StructType *stType = dyn_cast<StructType>(ElT1))
            //	errs()<<"ElT1 from candidate name:"<<stType->getName().str()<<"\n";
            //  errs()<<"ElT1 from candidate "<<*ElT1<<"\n";
            // errs()<<"ElT2 from caller: "<<*ElT2<<"\n";
            //return true;
            std::set<llvm::Function *> visit;
            return checkCastType(T1, T2, func, argNo, visit);
        }

        return isCompatibleType(ElT1, ElT2);
    }
    else if (T1->isArrayTy())
    {
        if (!T2->isArrayTy())
            return false;

        Type *ElT1 = T1->getArrayElementType();
        Type *ElT2 = T2->getArrayElementType();

        if (ElT1->isIntegerTy(8) /*|| ElT2->isIntegerTy(8)*/)
        {
            if (func && func->hasName())
                errs() << "Array Candidate function :" << func->getName().str() << "\n";
            //errs()<<"argNo = "<<argNo<<", inside isCompatibleType: \n";
            //if (const StructType *stType = dyn_cast<StructType>(ElT1))
            //      errs()<<"ElT1 from candidate name:"<<stType->getName().str()<<"\n";
            //  errs()<<"ElT1 from candidate "<<*ElT1<<"\n";
            // errs()<<"ElT2 from caller: "<<*ElT2<<"\n";
            //return true;
            std::set<llvm::Function *> visit;
            return checkCastType(T1, T2, func, argNo, visit);
        }

        return isCompatibleType(ElT1, ElT1);
    }
    else if (T1->isIntegerTy())
    {
        // assume pointer can be cased to the address space size
        if (T2->isPointerTy() && T1->getIntegerBitWidth() == T2->getPointerAddressSpace())
            return true;

        // assume all integer type are compatible
        if (T2->isIntegerTy())
            return true;
        else
            return false;
    }
    else if (T1->isStructTy())
    {
        StructType *ST1 = cast<StructType>(T1);
        StructType *ST2 = dyn_cast<StructType>(T2);
        if (!ST2)
            return false;

        // literal has to be equal
        if (ST1->isLiteral() != ST2->isLiteral())
            return false;

        // literal, compare content
        if (ST1->isLiteral())
        {
            unsigned numEl1 = ST1->getNumElements();
            if (numEl1 != ST2->getNumElements())
                return false;

            for (unsigned i = 0; i < numEl1; ++i)
            {
                if (!isCompatibleType(ST1->getElementType(i), ST2->getElementType(i)))
                    return false;
            }
            return true;
        }

        // not literal, use name?
        return ST1->getStructName().equals(ST2->getStructName());
    }
    else if (T1->isFunctionTy())
    {
        FunctionType *FT1 = cast<FunctionType>(T1);
        FunctionType *FT2 = dyn_cast<FunctionType>(T2);
        if (!FT2)
            return false;

        if (!isCompatibleType(FT1->getReturnType(), FT2->getReturnType()))
            return false;

        // assume varg is always compatible with varg?
        if (FT1->isVarArg())
        {
            if (FT2->isVarArg())
                return VAR_MATCH_VAR;
            else
                return !VAR_MATCH_VAR;
        }

        // compare args, again ...
        unsigned numParam1 = FT1->getNumParams();
        if (numParam1 != FT2->getNumParams())
            return false;

        for (unsigned i = 0; i < numParam1; ++i)
        {
            if (!isCompatibleType(FT1->getParamType(i), FT2->getParamType(i)))
                return false;
        }
        return true;
    }
    else
    {
        errs() << "Unhandled Types:" << *T1 << " :: " << *T2 << "\n";
        return T1->getTypeID() == T2->getTypeID();
    }
}
bool checkCastType(Type *T1, Type *T2, llvm::Function *F, int argNo, std::set<llvm::Function *> &Visit)
{
    if (!F || argNo < 0 || Visit.find(F) != Visit.end())
        return true;
    Visit.insert(F);
    errs() << "checkCastType for callee:  " << F->getName().str() << ", argNo = " << argNo << "\n";
    //errs()<<"check if type "<<*T1 <<" is compatible with "<<*T2<<"\n";

    int count = 0;
    llvm::Value *val = NULL;
    for (Argument &A : F->args())
    {
        if (argNo == count)
        {
            for (User *U : A.users())
            {
                if (StoreInst *SI = dyn_cast<StoreInst>(U))
                {
                    //errs()<<"SI :"<<*SI<<"\n";
                    llvm::Value *argaddr = SI->getOperand(1);
                    //errs()<<"argaddr: "<<*argaddr<<"\n";
                    for (User *Use : argaddr->users())
                    {
                        //errs()<<"Use: "<<*Use<<"\n";
                        if (LoadInst *LI = dyn_cast<LoadInst>(Use))
                        {
                            //errs()<<"LI :"<<*LI<<"\n";
                            for (User *LIUse : LI->users())
                            {
                                if (BitCastInst *BI = dyn_cast<BitCastInst>(LIUse))
                                {
                                    //errs()<<"BI : "<<*BI<<"\n";
                                    //Use BI to get the real type
                                    if (BI->getType()->getPointerElementType() != T1->getPointerElementType())
                                    {
                                        //errs()<<"Type "<<*BI->getType()->getPointerElementType()<<" is not compatible with "<<*T2->getPointerElementType()<<"\n";
                                        errs() << "Not Compatible.\n";
                                        return false;
                                    }
                                } //BI
                                  //check CI, if the T1 (from callee) is the string while the T2 (from caller) is a struct then it's  not compatible
                                if (CallInst *CI = dyn_cast<CallInst>(LIUse))
                                {
                                    //errs()<<"CI: "<<*CI<<"\n";
                                    if (CI->getCalledFunction())
                                    {
                                        Type *ElT2 = T2->getPointerElementType();
                                        if (const StructType *stType = dyn_cast<StructType>(ElT2))
                                        {
                                            std::string fName = CI->getCalledFunction()->getName().str();
                                            //errs()<<"fName: "<<fName<<"\n";
                                            if (StrFuncs.count(fName))
                                            {
                                                errs() << "Not Compatible.\n";
                                                return false;
                                            }
                                        }
                                        if (!getRealType(LI, CI, T1, T2, Visit))
                                        {
                                            return false;
                                        }
                                    } //CI->getCalledFunction
                                }     //CallInst *CI = dyn_cast<CallInst>(LIUse)
                            }
                        }
                    }
                }
            }
            break;
        }
        else
        {
            count++;
            if (count > argNo)
                break;
        }
    }
    //errs()<<"Compatible.\n";
    return true;
}
bool getRealType(llvm::Value *V, llvm::CallInst *CI, Type *T1, Type *T2, std::set<llvm::Function *> &Visit)
{
    //1.identify the argNo for V in CI
    //errs()<<"Inside getRealType : \n";
    int argNo = -1;
    llvm::Function *Callee = CI->getCalledFunction();
    //errs()<<"Callee ::"<<Callee->getName().str()<<"\n";
    for (int i = 0; i < CI->getNumArgOperands(); i++)
    {
        if (CI->getArgOperand(i) == V)
        {
            argNo = i;
            break;
        }
    }
    if (argNo < 0)
    {
        //errs()<<"argNo < 0\n";
        return true;
    }
    //errs()<<"argNo = "<<argNo<<"\n";
    return checkCastType(T1, T2, Callee, argNo, Visit);
}
std::string findVarName(const llvm::Value *V, llvm::Function *F)
{
    #ifdef dbg
    llvm::Value *src = NULL;
    std::queue<const Value *> EV;
    std::unordered_set<Value *> PV;
    EV.push(V);
    while (!EV.empty())
    {
        Value *Ins = EV.front();
        EV.pop();
        if (PV.count(Ins))
            continue;
        PV.insert(Ins);
        if (const Instruction *Ins = dyn_cast<Instruction>(V))
        {
            switch (Ins->getOpcode())
            {
            case Instruction::Load:
            {
                const LoadInst *LI = dyn_cast<LoadInst>(Ins);
                src = LI->getPointerOperand();
                break;
            }
            case Instruction::GetElementPtr:
            {
                const GetElementPtrInst *LI = dyn_cast<GetElementPtrInst>(Ins);
                src = GEPI->getPointerOperand();
                EV.push(src);
                break;
            }
            case Instruction::SExt:
            case Instruction::ZExt:
            case Instruction::BitCast:
            {
                src = Ins->getOperand(0);
            }
            } //switch(Ins->getOpcode())
        }
    }
    MDNode *MD = NULL;
    for (const_inst_iterator Iter = inst_begin(F), End = inst_end(F); Iter != End; ++Iter)
    {
        const Instruction *I = &*Iter;
        if (const DbgDeclareInst *DbgDeclare = dyn_cast<DbgDeclareInst>(I))
        {
            if (DbgDeclare->getAddress() == src)
                MD = DbgDeclare->getVariable();
        }
        else if (const DbgValueInst *DbgValue = dyn_cast<DbgValueInst>(I))
        {
            if (DbgValue->getValue() == src)
                MD = DbgValue->getVariable();
        }
    }
    if (MD)
    {
        return DIVariable(MD).getName();
    }
    #endif
    return "";
}
void findSources(const llvm::Value *V, std::set<const llvm::Value *> &SrcSet) {
	// If the CV is modified by the return value of a function
	const CallInst *CI = dyn_cast<CallInst>(V);
	if (CI) {
		Function *CF = CI->getCalledFunction();
		if (!CF) return;

		// Explicit handling of overflow operations, treat as binary operator
		if (CF->getName().contains("with.overflow")) {
			for (unsigned int i = 0; i < CI->getNumArgOperands(); ++i)
				findSources(CI->getArgOperand(i), SrcSet);
			return;
		}
		return;
	}

	if (dyn_cast<GlobalVariable>(V)) {
		// FIXME: ???
		const Constant *Ct = dyn_cast<Constant>(V);
		if (!Ct) return;

		SrcSet.insert(V);
		return;
	}
	// FIXME: constants

	if (dyn_cast<ConstantExpr>(V)) {
		const ConstantExpr *CE = dyn_cast<ConstantExpr>(V);
		findSources(CE->getOperand(0), SrcSet);
		return;
	}

	const LoadInst *LI = dyn_cast<LoadInst>(V);
	if (LI) {
		// FIXME: this is wrong
		findSources(LI->getPointerOperand(), SrcSet);
		return;
	}

	const SelectInst *SI = dyn_cast<SelectInst>(V);
	if (SI) {
		findSources(SI->getTrueValue(), SrcSet);
		findSources(SI->getFalseValue(), SrcSet);
		return;
	}

	const GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V);
	if (GEP) 
		return findSources(GEP->getPointerOperand(), SrcSet);


	const PHINode *PN = dyn_cast<PHINode>(V);
	if (PN) {
		for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
			Value *IV = PN->getIncomingValue(i);
			findSources(IV, SrcSet);
		}
		return;
	}

	const ICmpInst *ICmp = dyn_cast<ICmpInst>(V);
	if (ICmp) {
		for (unsigned i = 0; i < ICmp->getNumOperands(); ++i) {
			Value *Opd = ICmp->getOperand(i);
			findSources(Opd, SrcSet);
		}
		return;
	}

	const BinaryOperator *BO = dyn_cast<BinaryOperator>(V);
	if (BO) {
		for (unsigned i = 0, e = BO->getNumOperands(); i != e; ++i) {
			Value *Opd = BO->getOperand(i);
			if (dyn_cast<Constant>(Opd))
				continue;
			findSources(Opd, SrcSet);
		}
		return;
	}

	const UnaryInstruction *UI = dyn_cast<UnaryInstruction>(V);
	if (UI) {
		findSources(UI->getOperand(0), SrcSet);
		return;
	}

	if (dyn_cast<Constant>(V)) {
		SrcSet.insert(V);
		return;
	}
}
