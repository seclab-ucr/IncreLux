//#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include <llvm/Support/Debug.h>
#include <llvm/IR/DebugInfo.h>

#include "QualifierAnalysis.h"
#include "Annotation.h"
#include "Helper.h"
#include "Config.h"
#include "json11.hpp"
#include <cstring>
#include <deque>
#include <utility>

#define DEBUG_TITLE
#define WRITEJSON
#define W_TIMING
#define OP_DBG
//#define HEAP_CHECK
#define STACK_CHECK
//#define DUBUG_CHECK
//#define _PRINT_INST
//#define CALL_DBG
//#define WA_BC
//The file used for putting json obj
//std::string jsonfile = "/data/home/yizhuo/Pixel3/0206result/0206-exp.json";

//std::string jsonfile = "/data/home/yizhuo/Pixel3/1223-ind.json";
//std::string jsonfile = "/data/home/yizhuo/allyes-llbc-414/0101result/0101-ind.json";
//std::string jsonfile = "/data2/yizhuo/UBITect/incExpRes/lll-415-KRes/Warnings/415k.json";
//std::string jsonfile = "/data/home/yizhuo/experiment/ccs/0113/warnings.json";
//std::string jsonfile = "/data/home/yizhuo/incExp/minitest/kernelDiff/warn-415.json";
//std::string jsonfile = "/data/home/yizhuo/experiment/ccs/0116/warnings.json";
//std::string jsonfile = "/data/home/yizhuo/And-UBITect/testset/8bce/test.json";
//std::string jsonfile = "/data2/yizhuo/UBITect/incExpRes/lll-414-Res/Warnings/res-1.json";
//std::string jsonfile = "/data/home/yizhuo/incExp/minitest/kernelDiff/warn-415.json";
//std::string absPath = "/data/home/yizhuo/experiment/lll-414/";
//The abs of code for check
//std::string targetDir = "/data/home/yizhuo/Pixel3/";
//std::string targetDir = "/data/home/yizhuo/allyes-llbc-414/";
//std::unordered_map<int ,std::string> eToS = {{0, "FUNCTION_PTR"}, {1, "NORMAL_PTR"}, {2, "DATA"}, {3, "OTHER"}};
void FuncAnalysis::QualifierCheck()
{
    //OP<<"Inside QualifierCheck:\n";

    unsigned numNodes = nodeFactory.getNumNodes();
    Instruction *entry = &(F->front().front());
    NodeIndex entryIndex = nodeFactory.getValueNodeFor(entry);

    std::deque<Instruction *> worklist;
    for (inst_iterator itr = inst_begin(*F), ite = inst_end(*F); itr != ite; ++itr)
    {
        auto inst = itr.getInstructionIterator();
        worklist.push_back(&*inst);
    }
#ifdef DEBUG_TITLE
    OP << "Inside qualifier check:\n";
#endif
    std::set<const Instruction *> visit;
    std::string warningTy = "OTHER";

    while (!worklist.empty())
    {
        Instruction *I = worklist.front();
        worklist.pop_front();
#ifdef _PRINT_INST
        OP << *I << "\n";
#endif
#ifdef DUBUG_CHECK
        OP << *I << "\n";
        for (unsigned i = 0; i < numNodes; i++)
        {
            OP << "index = " << i << ", qualifier = " << nQualiArray[I].at(i) << "\n";
        }
#endif
        switch (I->getOpcode())
        {
        case Instruction::Load:
        case Instruction::GetElementPtr:
        {
            NodeIndex opIndex = nodeFactory.getValueNodeFor(I->getOperand(0));
            //OP<<"opIdx = "<<opIndex<<", qualifier = "<<nQualiArray[I][opIndex]<<"\n";
            assert(opIndex != AndersNodeFactory::InvalidIndex && "Failed to find load operand node");
            //NodeIndex valIndex = nodeFactory.getValueNodeFor(I);
            //assert(valIndex != AndersNodeFactory::InvalidIndex && "Failed to find load value node");
            //OP<<"opIndex = "<<opIndex<<", qualifier: "<<nQualiArray[I].at(opIndex)<<"\n";
#ifdef STACK_CHECK
            if (nQualiArray[I].at(opIndex) == _UD)
            {
                bool ans = warningReported(I, opIndex);
                if (ans)
                    break;
                
                OP << "***********Warning: potential uninitialized ptr" << *I->getOperand(0) << "\n";
                OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                visit.clear();
                warningTy = eToS[getWType(I->getOperand(0)->getType())];
#ifdef WA_BC
                addRelatedBC(I, opIndex);
#endif
                printRelatedBB(opIndex, I->getOperand(0), visit, warningTy);
                warningSet.insert(opIndex);
            }
#endif
            break;
        }
        case Instruction::Store:
        {
            NodeIndex srcIndex = nodeFactory.getValueNodeFor(I->getOperand(0));
            if (srcIndex == AndersNodeFactory::InvalidIndex)
            {
                // If we hit the store insturction like below, it is
                // possible that the global variable (init_user_ns) is
                // an extern variable. Hope this does not happen in kcfi,
                // otherwise we need to assume the worst case :-(

                // 1. store %struct.user_namespace* @init_user_ns,
                // %struct.user_namespace**  %from.addr.i, align 8
                // 2. @init_user_ns = external global %struct.user_namespace
                break;
            }
            // assert(srcIndex != AndersNodeFactory::InvalidIndex && "Failed to find store src node");
            NodeIndex dstIndex = nodeFactory.getValueNodeFor(I->getOperand(1));
            //OP<<"srcIndex = "<<srcIndex<<", dstIndex = "<<dstIndex<<"\n";
            assert(dstIndex != AndersNodeFactory::InvalidIndex && "Failed to find store dst node");
            bool heapNode = false;
            for (auto obj : nPtsGraph[I][dstIndex])
            {
                if (nodeFactory.isHeapNode(obj))
                    heapNode = true;
            }
            llvm::Type *type = NULL;
            if (I->getOperand(0)->getType()->isPointerTy())
            {
                type = cast<PointerType>(I->getOperand(0)->getType())->getElementType();
                // An array is considered a single variable of its type.
                while (const ArrayType *arrayType = dyn_cast<ArrayType>(type))
                    type = arrayType->getElementType();
            }
#ifdef HEAP_CHECK
            if (dstIndex == nodeFactory.getUniversalPtrNode() || heapNode)
            {
                if (nQualiArray[I].at(srcIndex) != _ID)
                {
                    std::ofstream ofile;
                    ofile.open(Ctx->heapWarning, std::ios::app);
                    OP << "***********Heap Warning: uninitialized ptr stored to  heap" << *I->getOperand(0) << "\n";
                    OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                    OP << "***********Heap Warning: uninitialized ptr stored to  heap" << *I->getOperand(0) << "\n";
                    OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                    OP << "Module: " << F->getParent()->getName().str() << "\n";
                    visit.clear();
                    printRelatedBB(srcIndex, I, visit);
                }

                if (I->getOperand(0)->getType()->isPointerTy())
                {
                    for (auto obj : nPtsGraph[I][srcIndex])
                    {
                        if (obj <= nodeFactory.getConstantIntNode())
                            continue;
                        unsigned objSize = nodeFactory.getObjectSize(obj);
                        unsigned objOffset = nodeFactory.getObjectOffset(obj);
                        //OP<<"obj = "<<obj<<", objSize = "<<objSize<<", objOffsetf = "<<objOffset<<"\n";
                        if (const StructType *structType = dyn_cast<StructType>(type))
                        {
                            for (unsigned i = 0; i < objSize - objOffset; i++)
                            {
                                if (nQualiArray[I].at(obj + i) != _ID)
                                {
                                    std::ofstream ofile;
                                    ofile.open(Ctx->heapWarning, std::ios::app);
                                    if (Ctx->usedField[structType].find(i) == Ctx->usedField[structType].end())
                                        continue;
                                    OP << "***********Heap Warning: uninitialized data stored to  heap:\n";
                                    OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                                    OP << "field " << i << "\n";
                                    OP << "***********Heap Warning: uninitialized data stored to  heap:\n";
                                    OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                                    OP << "field " << i << "\n";
                                    OP << "Module: " << F->getParent()->getName().str() << "\n";
                                    visit.clear();
                                    printRelatedBB(obj + i, I, visit);
                                }
                            }
                        }
                        else
                        {
                            if (nQualiArray[I].at(obj) != _ID)
                            {
                                std::ofstream ofile;
                                ofile.open(Ctx->heapWarning, std::ios::app);
                                OP << "***********Heap Warning: uninitialized data stored to  heap:\n";
                                OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                                OP << "***********Heap Warning: uninitialized data stored to  heap:\n";
                                OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                                OP << "Module: " << F->getParent()->getName().str() << "\n";
                                visit.clear();
                                printRelatedBB(obj, I, visit);
                            }
                        }
                    }
                }
            }
#endif
            break;
        }
        case Instruction::ICmp:
        {
            //%7 = icmp eq op0, op1
            NodeIndex op0Index = nodeFactory.getValueNodeFor(I->getOperand(0));
            NodeIndex op1Index = nodeFactory.getValueNodeFor(I->getOperand(1));
            assert(op0Index != AndersNodeFactory::InvalidIndex && "Failed to find node for op0");
            assert(op1Index != AndersNodeFactory::InvalidIndex && "Failed to find node for op1");
//OP<<"op0Index = "<<op0Index<<" : "<<nQualiArray[I][op0Index]<<"\n";
#ifdef STACK_CHECK
            if (nQualiArray[I].at(op0Index) == _UD)
            {
                //OP<<"op0Index:"<<op0Index<<" is uninitialized\n";
                bool ans = warningReported(I, op0Index);
                if (!ans)
                {
                    OP << "***********Warning: uninitialized ptr " << *I->getOperand(0) << "\n";
                    OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                    warningTy = eToS[getWType(I->getOperand(0)->getType())];
                    //setRelatedBB(op0Index, I->getParent()->getName().str());
                    visit.clear();
#ifdef WA_BC
                    addRelatedBC(I, op0Index);
#endif
                    printRelatedBB(op0Index, I->getOperand(0), visit, warningTy);
                    warningSet.insert(op1Index);
                }
            }
            if (nQualiArray[I].at(op1Index) == _UD)
            {
		//OP<<"op1Index = "<<op1Index<<" : "<<nQualiArray[I][op1Index]<<"\n";
                bool ans = warningReported(I, op1Index);
                if (ans)
                    break;
                OP << "***********Warning: uninitialized ptr " << *I->getOperand(1) << "\n";
                OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                warningTy = eToS[getWType(I->getOperand(1)->getType())];
                //setRelatedBB(op1Index, I->getParent()->getName().str());
                visit.clear();
#ifdef WA_BC
                addRelatedBC(I, op1Index);
#endif
                printRelatedBB(op1Index, I->getOperand(1), visit, warningTy);
                warningSet.insert(op1Index);
            }
#endif
            break;
        }
        case Instruction::Switch:
        {
            NodeIndex conIndex = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(conIndex != AndersNodeFactory::InvalidIndex && "Failed to find node for condition");
#ifdef STACK_CHECK
            if (nQualiArray[I].at(conIndex) == _UD)
            {
                bool ans = warningReported(I, conIndex);
                if (ans)
                    break;
                OP << "***********Warning: uninitialized ptr " << *I->getOperand(0) << "\n";
                OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                warningTy = eToS[getWType(I->getOperand(0)->getType())];
                //setRelatedBB(conIndex, I->getParent()->getName().str());
                visit.clear();
#ifdef WA_BC
                addRelatedBC(I, conIndex);
#endif
                printRelatedBB(conIndex, I->getOperand(0), visit, warningTy);
                warningSet.insert(conIndex);
            }
#endif
            break;
        }
        case Instruction::Call:
        {
            if (isa<DbgInfoIntrinsic>(I))
            {
                //OP<<"do not check dbgInfoIntrinsic: "<<*I<<"\n";
                break;
            }
            CallInst *CI = dyn_cast<CallInst>(I);
#ifdef CALL_DBG
            for (int argNo = 0; argNo < CI->getNumArgOperands(); argNo++)
            {
                NodeIndex argIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(argNo));
                if (!(CI->getArgOperand(argNo)->getType())->isPointerTy())
                    continue;
                //OP<<"argIndex = "<<argIndex<<"\n";
                const Type *type = cast<PointerType>(CI->getArgOperand(argNo)->getType())->getElementType();

                // An array is considered a single variable of its type.
                while (const ArrayType *arrayType = dyn_cast<ArrayType>(type))
                    type = arrayType->getElementType();

                if (const StructType *structType = dyn_cast<StructType>(type))
                {
                    if (structType->isOpaque())
                    {
                        continue;
                    }

                    const StructInfo *stInfo = Ctx->structAnalyzer.getStructInfo(structType, M);
                    uint64_t stSize = stInfo->getExpandedSize();
                    OP << "qualifier of argIndex: " << nQualiArray[I].at(argIndex) << "\n";
                    for (auto obj : nPtsGraph[I][argIndex])
                    {
                        for (uint64_t i = 0; i < stSize; i++)
                            OP << "qualifier of node in[" << obj + i << "] = " << nQualiArray[I].at(obj + i) << "\n";
                    }
                }
            }
#endif

            //1.check the input of assembly
            if (CI->isInlineAsm())
            {
                InlineAsm *ASM = dyn_cast<InlineAsm>(CI->getCalledValue());
                InlineAsm::ConstraintInfoVector CIV = ASM->ParseConstraints();

                bool init = true;
                for (int argNo = 0; argNo < CI->getNumArgOperands(); argNo++)
                {
                    NodeIndex argIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(argNo));
                    Value *argValue = CI->getArgOperand(argNo);

                    InlineAsm::ConstraintInfo consInfo = CIV.at(argNo);
                    if (consInfo.Type == InlineAsm::isInput)
                    {
                        if (nQualiArray[I].at(argIndex) == _UD)
                        {
                            insertUninit(I, argIndex, uninitArg);
#ifdef STACK_CHECK
                            bool ans = warningReported(I, argIndex);
                            if (ans)
                                continue;

                            //OP<<"argIndex = "<<argIndex<<", quali : "<<nQualiArray[I][argIndex]<<"\n";
                            OP << "***********Warning: Asm Input not init at arg  " << argNo << "\n";
                            OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                            //setRelatedBB(argIndex, I->getParent()->getName().str());
                            OP << "argIndex = " << argIndex << ", value: " << *CI->getArgOperand(argNo) << "\n";
                            warningTy = eToS[getWType(CI->getArgOperand(argNo)->getType())];
                            visit.clear();
#ifdef WA_BC
                            addRelatedBC(I, argIndex);
#endif
                            printRelatedBB(argIndex, I, visit, warningTy);
                            warningSet.insert(argIndex);
#endif
                        }
                    }
                    //const std::string str = ASM->getConstraintString();
                }
                break;
            }
            //2. skip the heap allocation function
            if (CI->getCalledFunction())
            {
                StringRef fName = CI->getCalledFunction()->getName();
                if (Ctx->HeapAllocFuncs.count(fName.str()))
                {
                    break;
                }
            }
            //3. direct call check
#ifdef STACK_CHECK
            if (CI->getCalledFunction())
            {
                Function *Func = CI->getCalledFunction();
                StringRef fName = Func->getName();

                if (Ctx->OtherFuncs.count(fName.str()))
                {
                    //do nothing here
                }
                else if (Ctx->InitFuncs.count(fName.str()) || Ctx->PreSumFuncs.count(fName.str())|| fName.str().find("print") != std::string::npos)
                {
                    for (int argNo = 0; argNo < CI->getNumArgOperands(); argNo++)
                    {
                        NodeIndex argIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(argNo));
                        if (nQualiArray[I].at(argIndex) == _UD)
                        {
                            insertUninit(I, argIndex, uninitArg);
                            bool ans = warningReported(I, argIndex);
                            if (ans)
                                continue;

                            OP << "***********Warning: argument check fails @ argument " << argNo << " : arg not initialized\n";
                            OP << "[trace] In function @" << fName.str() << " Instruction:" << *I << "\n";
                            //setRelatedBB(argIndex, I->getParent()->getName().str());
                            visit.clear();
                            warningTy = eToS[getWType(CI->getArgOperand(argNo)->getType())];
                            printRelatedBB(argIndex, I, visit, warningTy);
                            warningSet.insert(argIndex);
                        }
                    }
                }
                else if (Ctx->CopyFuncs.count(fName.str()))
                {
                    checkCopyFuncs(I, Func);
                }
                else if (Ctx->TransferFuncs.count(fName.str()))
                {
                    checkTransferFuncs(I, Func);
                }
		else if (fName.str().find("llvm_gcov") != std::string::npos)
		{
			//do nothing
		}
                else if (Ctx->ObjSizeFuncs.count(fName.str()))
                {
                    NodeIndex argIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(0));
                    if (nQualiArray[I].at(argIndex) == _UD)
                    {
                        insertUninit(I, argIndex, uninitArg);
                        if (!warningReported(I, argIndex))
                        {
                            OP << "***********Warning: argument check fails @ argument 0: arg not initialized\n";
                            OP << "[trace] In function @" << fName.str() << " Instruction:" << *I << "\n";
                            //setRelatedBB(argIndex, I->getParent()->getName().str());
                            warningTy = eToS[getWType(CI->getArgOperand(0)->getType())];
                            visit.clear();
                            printRelatedBB(argIndex, I, visit, warningTy);
                            warningSet.insert(argIndex);
                        }
                    }
                }
                else if (Ctx->OtherFuncs.count(fName.str()))
                {
                    continue;
                }
                else
                {
                    //OP<<"getCalledFunction :"<<CI->getCalledFunction()<<": "<<CI->getCalledFunction()->getName().str()<<"\n";
                    //OP << "check func: " << fName << ": " << Func << "\n";
                    checkFuncs(I, Func);
                }
            }
            else
            {
                //indirect call
                //OP<<"Indirect call:\n";
                for (auto Func : Ctx->Callees[CI])
                {
                    StringRef fName = Func->getName();
                    //OP << "possible function: " << fName.str() << "\n";
                    if (Ctx->OtherFuncs.count(fName.str()))
                    {
                        continue;
                    }
                    else if (Ctx->PreSumFuncs.count(fName.str()) || Ctx->InitFuncs.count(fName.str()))
                    {
                        for (int argNo = 0; argNo < CI->getNumArgOperands(); argNo++)
                        {
                            NodeIndex argIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(argNo));
                            if (nQualiArray[I].at(argIndex) == _UD)
                            {
                                insertUninit(I, argIndex, uninitArg);
                                if (!warningReported(I, argIndex))
                                {
                                    OP << "***********Warning: argument check fails @ argument " << argNo << " : arg not initialized\n";
                                    OP << "[trace] In function @" << fName.str() << " Instruction:" << *I << "\n";
                                    warningTy = eToS[getWType(CI->getArgOperand(argNo)->getType())];
                                    //setRelatedBB(argIndex, I->getParent()->getName().str());
                                    visit.clear();
                                    printRelatedBB(argIndex, I, visit, warningTy);
                                    warningSet.insert(argIndex);
                                }
                            }
                        }
                    }
                    else if (Ctx->CopyFuncs.count(fName.str()))
                    {
                        checkCopyFuncs(I, Func);
                    }
                    else if (Ctx->TransferFuncs.count(fName.str()))
                    {
                        checkTransferFuncs(I, Func);
                    }
		    else if (fName.str().find("llvm_gcov") != std::string::npos)
		    {
						                //do nothing
		    }
                    else if (Ctx->ObjSizeFuncs.count(fName.str()))
                    {
                        NodeIndex argIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(0));
                        if (nQualiArray[I].at(argIndex) == _UD)
                        {
                            insertUninit(I, argIndex, uninitArg);
                            if (!warningReported(I, argIndex))
                            {
                                OP << "***********Warning: argument check fails @ argument 0: arg not initialized\n";
                                OP << "[trace] In function @" << fName.str() << " Instruction:" << *I << "\n";
                                //setRelatedBB(argIndex, I->getParent()->getName().str());
                                warningTy = eToS[getWType(CI->getArgOperand(0)->getType())];
                                visit.clear();
                                printRelatedBB(argIndex, I, visit, warningTy);
                            }
                        }
                    }
                    else
                    {
                        checkFuncs(I, Func);
                    }
                }
            }
#endif
            break;
        }
        default:
        {
            break;
        }
        }
    }
}
//nodeIndex: printRelatedBB for nodeIndex, check the define before Instruction I
void FuncAnalysis::printRelatedBB(NodeIndex nodeIndex, const llvm::Value *Val,
                                  std::set<const Instruction *> &v, std::string rank, int argNo, int field, llvm::Function *Callee)
{
    OP<<"Inside printRelatedBB:\n";
    const llvm::Instruction *I = dyn_cast<const llvm::Instruction>(Val);

    if (!I)
    {
        OP << "Warning but no inst.\n";
        return;
    }
    std::set<const BasicBlock *> blacklist;
    std::set<const BasicBlock *> whitelist;
    std::set<NodeIndex> visit;
    std::set<const BasicBlock *> useBlacklist;
    std::set<const BasicBlock *> altBlacklist;

    //calculate each list:

    calculateRelatedBB(nodeIndex, I, visit, blacklist, whitelist);
    calculateBLForUse(I, useBlacklist);

    BasicBlock *entry = &(F->front());
    json11::Json::array whiteArr;
    json11::Json::array blackArr;
    json11::Json::array altBlackArr;
    DominatorTree T(*F);
    std::set<const BasicBlock *> newBlacklist = blacklist;

    const llvm::BasicBlock *entryBB = &(F->front());

    for (const BasicBlock *B : blacklist)
    {
        for (const BasicBlock *witem : whitelist)
        {
            if (T.dominates(B, witem))
            {
                //OP<<B->getName().str()<<" dominates "<<witem->getName().str()<<"\n";
                altBlacklist.insert(B);
                newBlacklist.erase(B);
                break;
            }
        }
    }
    for (auto witem : whitelist)
    {
        if ((I->getParent() != entryBB) && (witem == I->getParent()))
            continue;
        //OP<<"---whitelist: "<<witem->getName().str()<<"\n";
        whiteArr.push_back(json11::Json(witem->getName().str()));
    }
    for (auto bitem : newBlacklist)
    {
        //OP<<"--blacklist: "<<bitem->getName().str()<<"\n";
        blackArr.push_back(json11::Json(bitem->getName().str()));
    }
    for (auto bitem : useBlacklist)
    {
        //OP<<"--blacklist: "<<bitem->getName().str()<<"\n";
        blackArr.push_back(json11::Json(bitem->getName().str()));
    }
    for (auto bitem : altBlacklist)
    {
        //OP<<"--alt-blacklist: "<<bitem->getName().str()<<"\n";
        altBlackArr.push_back(json11::Json(bitem->getName().str()));
    }
    //get the list of the node
    for (auto aa : nAAMap[I][nodeIndex])
    {
        for (auto item : nodeFactory.getWL(aa))
        {
            whiteArr.push_back(json11::Json(item));
        }
        for (auto item : nodeFactory.getBL(aa))
        {
            blackArr.push_back(json11::Json(item));
        }
    }
    //get the list for the argument
    if (Callee != NULL)
    {
        int sumArgNo = argNo + 1;
        for (auto sumObj : Ctx->FSummaries[Callee].sumPtsGraph[sumArgNo])
        {
            int sumArgIdx = sumObj + field;
            for (auto bitem : Ctx->FSummaries[Callee].args[sumArgIdx].getBlackList())
            {
                blackArr.push_back(json11::Json(bitem));
            }
            for (auto witem : Ctx->FSummaries[Callee].args[sumArgIdx].getWhiteList())
            {
                whiteArr.push_back(json11::Json(witem));
            }
        }
    }
    //OP<<"-use: "<<I->getParent()->getName().str()<<"\n";
    //change it to json format
    std::string moduleName = Ctx->funcToMd[I->getParent()->getParent()].str();
    //OP<<"moduleName = "<<moduleName<<"\n";
    int numAlias = nAAMap[I][nodeIndex].size();
    //int start = targetDir.size();
    //std::string path = moduleName.substr(start);
    std::string insStr;
    llvm::raw_string_ostream rso(insStr);
    I->print(rso);
    //std::string id = moduleName+"_"+I->getParent()->getParent()->getName().str()+"_"+insStr+"_"+std::to_string(argNo)+"_"+std::to_string(field);
    std::string id = nodeFactory.getNodeName(nodeIndex);

    std::string cur=Ctx->oldVersion;
    cur.pop_back();
    cur += "/";
    std::string newv=Ctx->newVersion;
    newv.pop_back();
    newv += "/";
    OP<<"moduleName1:"<<moduleName<<"\n";
    size_t pos = moduleName.find(newv);
    if (pos  != std::string::npos) {
        OP<<"case1:\n";
        moduleName = moduleName.substr(Ctx->absPath.size(), moduleName.size());
        OP<<"moduleName1.2:"<<moduleName<<"\n";
    }
    else {
        OP<<"case2:\n";
        pos = moduleName.find(cur);
        if (pos  != std::string::npos) {
            moduleName.replace(pos, cur.size(), newv);

            OP<<"moduleName1.5:"<<moduleName<<"\n";
            pos = moduleName.find(newv);
            if (pos  != std::string::npos) {
                moduleName = moduleName.substr(Ctx->absPath.size(), moduleName.size());
            }
        }
    }

    OP << "print related BB, moduleName = " << moduleName << "\n";

    //if (moduleName.size() > Ctx->absPath.size())
    //    moduleName = moduleName.substr(Ctx->absPath.size(), moduleName.size());
    //OP << "moduleName = " << moduleName << "\n";
    const llvm::DebugLoc &loc = I->getDebugLoc();
    int line = -1;
    int col = -1;
    if (loc)
    {
        line = loc.getLine();
        col = loc.getCol();
    }
    //OP << "line = " << line << ", col = " << col << "\n";
    id = moduleName + "_" + I->getParent()->getParent()->getName().str() + "_" + id;
    std::string  varName= findVarName(Val, F);
    #ifdef varName
    std::set<const llvm::Value*>SrcSet;
    findSources(Val, SrcSet);
    OP<<"Val: "<<*Val<<": srcSet:\n";
    for (auto item : SrcSet) {
        OP<<*item<<"\n";
    }
    OP<<"\n";
    #endif
    json11::Json jsonObj = json11::Json::object{
        {"id", id},
        {"bc", moduleName},
        {"type", "stack"},
        {"whitelist", json11::Json(whiteArr)},
        {"blacklist", json11::Json(blackArr)},
        {"alt-blist", json11::Json(altBlackArr)},
        {"use", I->getParent()->getName().str()},
        {"function", I->getParent()->getParent()->getName().str()},
        {"warning", insStr},
        {"rank", rank},
        {"argNo", argNo},
        {"fieldNo", field},
        {"aliasNum", numAlias},
        {"lineNo", itostr(line)},
        {"colNo", itostr(col)},
        //{"variable", varName}
    };
    std::string json_str = jsonObj.dump();
    for (auto item : fSummary.relatedBC)
    {
        OP << item << ":";
    }
    OP << "\n";
    OP << "json obj:" << json_str << "\n";

    std::string bcstr = "";
    //for (auto item : Ctx->FSummaries[F].relatedBC)
    //{
    //    bcstr += item;
    //    bcstr += ":";
    //}
    for (auto item : fSummary.relatedBC) {
        bcstr += item;
        bcstr += ":";
    }
    
    if (printWarning == false) {
        //OP<<"insert ("<<bcstr<<" \n";
        //OP<<json_str<<")\n";
        Ctx->fToWarns[F].insert(std::make_pair(bcstr, json_str));
        return;
    }
#ifdef WRITEJSON
#ifdef W_TIMING
    clock_t sTime, eTime;
    sTime = clock();
#endif
    std::ofstream jfile;
    if (Ctx->incAnalysis){
        jfile.open(Ctx->jsonInc, std::ios::app);
    }
    else {
        jfile.open(Ctx->jsonFile, std::ios::app);
    }
        
    //print the related bc files:
    jfile << bcstr << "\n";
    jfile << json_str << "\n";
    jfile.close();
#ifdef W_TIMING
    //OP<<"warnings: \n";
    //OP<<bcstr<<"\n";
    //OP<<json_str<<"\n";
    eTime = clock();
    OP<<"JSON write cost " << (double)(eTime - sTime) / CLOCKS_PER_SEC<<"{s}\n";
#endif
#endif
}
void FuncAnalysis::calculateBLForUse(const llvm::Instruction *I, std::set<const BasicBlock *> &blacklist)
{
    //OP<<"Inside calculateBLForUse:\n";
    if (!dyn_cast<Instruction>(I))
        return;
    OP << *I << "\n";
    const llvm::BasicBlock *BB = I->getParent();

    std::set<const BasicBlock *> predList;
    std::deque<const llvm::BasicBlock *> BBQ;
    std::set<const BasicBlock *> visit;
    BBQ.push_back(BB);
    predList.insert(BB);
    while (!BBQ.empty())
    {
        const llvm::BasicBlock *current = BBQ.front();
        BBQ.pop_front();
        visit.insert(current);
        for (auto it = pred_begin(current), et = pred_end(current); it != et; ++it)
        {
            const BasicBlock *pred = *it;
            predList.insert(pred);
            //OP<<"insert "<<pred->getName().str()<<"\n";
            //insert the element if it's not visited before and not in the dequeue
            if (visit.find(pred) == visit.end())
            {
                std::deque<const llvm::BasicBlock *>::iterator it = find(BBQ.begin(), BBQ.end(), pred);
                if (it == BBQ.end())
                {
                    //OP<<"push to BBQ: "<<pred<<": "<<pred->getName().str()<<"\n";
                    BBQ.push_back(pred);
                }
            }
        }
    }

    for (Function::iterator bb = F->begin(), be = F->end(); bb != be; ++bb)
    {
        BasicBlock *it = &*bb;
        if (predList.find(it) == predList.end())
        {
            //OP<<"blacklist insert: "<<it->getName().str()<<"\n";
            blacklist.insert(it);
        }
    }
}
void FuncAnalysis::calculateRelatedBB(NodeIndex nodeIndex, const llvm::Instruction *I, std::set<NodeIndex> &visit,
                                      std::set<const BasicBlock *> &blacklist, std::set<const BasicBlock *> &whitelist)
{
    //OP<<"calculatedRelatedBB for nodeIndex : "<<nodeIndex<<", Ins: "<<*I<<"\n";
    //if the NodeIndex is calculated before, then return
    if (nodeIndex <= nodeFactory.getConstantIntNode() || I == NULL)
        return;
    if (visit.find(nodeIndex) != visit.end())
        return;
    const llvm::Value *value = nodeFactory.getValueForNode(nodeIndex);
    visit.insert(nodeIndex);

    const llvm::BasicBlock *entry = &(F->front());
    whitelist.insert(entry);
    //check the blacklist for nodeIndex
    for (auto aa : nAAMap[I][nodeIndex])
    {
        const llvm::Value *aaValue = nodeFactory.getValueForNode(aa);
        if (aaValue)
        {
            if (const Instruction *aaIns = dyn_cast<Instruction>(aaValue))
            {
                if (nodeFactory.isValueNode(aa) && isa<AllocaInst>(aaIns))
                {
                    //OP<<"alloca Inst.\n";
                    continue;
                }

                if (nQualiArray[aaIns].at(aa) == _UD)
                {
                    //OP<<"whitelist insert: "<<aaIns->getParent()->getName().str()<<"\n";
                    //whitelist.insert(aaIns->getParent());
                    calculateRelatedBB(aa, aaIns, visit, blacklist, whitelist);
                }
                if (nQualiArray[aaIns].at(aa) == _ID)
                {
                    //OP<<"Insert to blacklist: "<<aaIns->getParent()->getName().str()<<"\n";
                    //blacklist.insert(aaIns->getParent());
                    calculateRelatedBB(aa, aaIns, visit, blacklist, whitelist);
                }
            }
            //seperatet the quali req and therefore we can scan the BB.
            /*		
		if (inQualiArray[aaIns->getParent()].at(aa) == _UD && outQualiArray[aaIns->getParent()].at(aa) == _ID) {
			OP<<"blacklist insert :"<<aaIns->getParent()->getName().str()<<"\n";
			blacklist.insert(aaIns->getParent());
		}*/

            for (Function::iterator bb = F->begin(), be = F->end(); bb != be; bb++)
            {
                const BasicBlock *BB = &*bb;
                //OP<<"BBname: "<<BB->getName().str()<<", in: "<<inQualiArray[BB][aa]<<", out :"<<outQualiArray[BB][aa]<<"\n";
                if (inQualiArray[BB].at(aa) != _ID && outQualiArray[BB].at(aa) == _ID)
                {
                    //OP<<"Insert to blacklist: "<<BB->getName().str()<<"\n";
                    blacklist.insert(BB);
                }
		if (inQualiArray[BB].at(aa) == _ID && outQualiArray[BB].at(aa) == _UD){
		    whitelist.insert(BB);
		}
            }

            if (const Instruction *Inst = dyn_cast<Instruction>(aaValue))
            {
                switch (Inst->getOpcode())
                {
                case Instruction::Load:
                {
                    NodeIndex srcNode = nodeFactory.getValueNodeFor(Inst->getOperand(0));
                    if (nQualiArray[Inst].at(srcNode) == _UD)
                    {
                        calculateRelatedBB(srcNode, Inst, visit, blacklist, whitelist);
                    }
                    break;
                }
                }
            }
        }
    }
//blacklist:
#ifdef blacklist
    for (Function::iterator bb = F->begin(), be = F->end(); bb != be; bb++)
    {
        BasicBlock *BB = &*bb;
        for (auto aa : nAAMap[I][nodeIndex])
        {
            //OP<<"aa = "<<aa<<"\n";
            bool trace = false;
            //OP<<"BB : "<<BB->getName().str()<<", in : "<<inQualiArray[BB][aa]<<", out : "<<outQualiArray[BB][aa]<<"\n";

            if (inQualiArray[BB].at(aa) != _ID && outQualiArray[BB].at(aa) == _ID)
            {
                blacklist.insert(BB);
                trace = true;
            }
            if (trace)
            {
                //OP<<"value: "<<*value<<"\n";
                if (const Instruction *Inst = dyn_cast<Instruction>(value))
                {
                    switch (Inst->getOpcode())
                    {
                    case Instruction::Add:
                    case Instruction::FAdd:
                    case Instruction::Sub:
                    case Instruction::FSub:
                    case Instruction::Mul:
                    case Instruction::FMul:
                    case Instruction::SDiv:
                    case Instruction::UDiv:
                    case Instruction::FDiv:
                    case Instruction::SRem:
                    case Instruction::URem:
                    case Instruction::And:
                    case Instruction::Or:
                    case Instruction::Xor:
                    case Instruction::LShr:
                    case Instruction::AShr:
                    case Instruction::Shl:
                    case Instruction::ICmp:
                    {
                        NodeIndex dstIndex = nodeFactory.getValueNodeFor(Inst);
                        NodeIndex op0Index = nodeFactory.getValueNodeFor(Inst->getOperand(0));
                        NodeIndex op1Index = nodeFactory.getValueNodeFor(Inst->getOperand(1));
                        if (nQualiArray[Inst].at(op0Index) == _UD)
                            calculateRelatedBB(op0Index, Inst, visit, blacklist, whitelist);
                        if (nQualiArray[Inst].at(op1Index) == _UD)
                            calculateRelatedBB(op0Index, Inst, visit, blacklist, whitelist);
                        break;
                    }
                    case Instruction::GetElementPtr:
                    case Instruction::SExt:
                    case Instruction::ZExt:
                    case Instruction::BitCast:
                    case Instruction::Trunc:
                    case Instruction::IntToPtr:
                    case Instruction::PtrToInt:
                    case Instruction::ExtractValue:
                    {
                        NodeIndex srcIndex = nodeFactory.getValueNodeFor(Inst->getOperand(0));
                        if (nQualiArray[Inst].at(srcIndex) == _UD)
                            calculateRelatedBB(srcIndex, Inst, visit, blacklist, whitelist);
                        break;
                    }
                    case Instruction::PHI:
                    {
                        const PHINode *phiInst = cast<PHINode>(Inst);
                        NodeIndex dstIndex = nodeFactory.getValueNodeFor(phiInst);
                        //OP<<"incomming #: "<<phiInst->getNumIncomingValues()<<"\n";
                        for (unsigned i = 0; i < phiInst->getNumIncomingValues(); i++)
                        {
                            NodeIndex srcIndex = nodeFactory.getValueNodeFor(phiInst->getIncomingValue(i));
                            if (nQualiArray[Inst].at(srcIndex) == _UD)
                                calculateRelatedBB(srcIndex, Inst, visit, blacklist, whitelist);
                        }
                        break;
                    }
                    } //switch
                }
            }
        }
    }
#endif
}
enum WarnType FuncAnalysis::getWType(llvm::Type *wType)
{
    Type *Ty = wType;
    if (PointerType *PT = dyn_cast<PointerType>(Ty))
    {
        do
        {
            Type *ptdTy = PT->getElementType();
            if (ptdTy->isFunctionTy())
            {
                return FUNCTION_PTR;
            }
            Ty = ptdTy;
        } while ((PT = dyn_cast<PointerType>(Ty)));
        return NORMAL_PTR;
    }
    else
    {
        return DATA;
    }
}
enum WarnType FuncAnalysis::getFieldTy(llvm::Type *wType, int field)
{
    return NORMAL_PTR;
    /*Type *Ty = wType;
	if (PointerType * PT = dyn_cast<PointerType>(Ty)) {
		Type *trueEleTy = PT->getElementType();
		while (const ArrayType *arrayType= dyn_cast<ArrayType>(trueEleTy))
        		trueEleTy = arrayType->getElementType();
		if (trueEleTy->isStructTy()) {
			StructType* stType = cast<StructType>(trueEleTy);
			OP<<"stType: "<<*stType<<"\n";
			const StructInfo* stInfo = Ctx->structAnalyzer.getStructInfo(stType, F->getParent());
			OP<<"field: "<<field<<", numElements: "<<stInfo->getSize()<<"\n";
			trueEleTy = stType->getElementType(field);
			if (trueEleTy->isFunctionTy()) {
                       		return FUNCTION_PTR;
                        }
		}
		return NORMAL_PTR;
	}
	else {
		return DATA;
	}*/
}
void FuncAnalysis::insertUninit(const llvm::Instruction *I, NodeIndex idx, std::set<NodeIndex> &uninitSet)
{
    //check if the idx node is on stack
    Instruction *entry = &(F->front().front());
    NodeIndex entryIndex = nodeFactory.getValueNodeFor(entry);

    for (auto aa : nAAMap[I][idx])
    {
        if (aa < entryIndex)
            return;
        const llvm::Value *aaVal = nodeFactory.getValueForNode(aa);
        if (aaVal != NULL)
        {
            if (const Instruction *Ins = dyn_cast<Instruction>(aaVal))
            {
                if (const AllocaInst *AI = dyn_cast<AllocaInst>(Ins))
                {
                    break;
                }
            }
        }
        if (uninitSet.find(aa) != uninitSet.end())
            return;
    }
    //OP<<"insert "<<idx<<"\n";
    uninitSet.insert(idx);
}
void FuncAnalysis::addRelatedBC(llvm::Instruction *I, NodeIndex idx, llvm::Function *Callee)
{
    for (auto item : Ctx->FSummaries[Callee].relatedBC)
    {
        fSummary.relatedBC.insert(item);
    }

#ifdef warningRelated
    //if Callee != Null, then the warning is not related to the function call
    if (Callee)
    {
        fSummary.relatedBC.insert(Callee->getParent()->getName().str());
    }
    for (inst_iterator itr = inst_begin(*F), ite = inst_end(*F); itr != ite; ++itr)
    {
        auto inst = itr.getInstructionIterator();
        if (CallInst *CI = dyn_cast<CallInst>(&*inst))
        {
            for (int argNo = 0; argNo < CI->getNumArgOperands(); argNo++)
            {
                NodeIndex argIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(argNo));
                if (nAAMap[I][idx].find(argIndex) != nAAMap[I][idx].end())
                {
                    if (CI->getFunction())
                    {
                        fSummary.relatedBC.insert(CI->getFunction()->getParent()->getName().str());
                        return;
                    }
                }
            }
        }
    }
#endif
}
