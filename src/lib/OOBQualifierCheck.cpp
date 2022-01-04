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
#define WRITEJSON
void FuncAnalysis::oobQualifierCheck() {
    OP<<"Inside ioQualifierCheck():\n";
    unsigned numNodes = nodeFactory.getNumNodes();
    Instruction *entry = &(F->front().front());
    NodeIndex entryIndex = nodeFactory.getValueNodeFor(entry);

    std::deque<Instruction *> worklist;
    for (inst_iterator itr = inst_begin(*F), ite = inst_end(*F); itr != ite; ++itr)
    {
        auto inst = itr.getInstructionIterator();
        worklist.push_back(&*inst);
    }
    std::set<const Instruction *> visit;

    while (!worklist.empty()) {
        Instruction *I = worklist.front();
        worklist.pop_front();
        switch (I->getOpcode()) {
            case Instruction::Call:{
                if (isa<DbgInfoIntrinsic>(I))
                {
                    //OP<<"do not check dbgInfoIntrinsic: "<<*I<<"\n";
                    break;
                }
                CallInst *CI = dyn_cast<CallInst>(I);

                if (CI->getCalledFunction())
                {
                    Function *Func = CI->getCalledFunction();
                    StringRef fName = Func->getName();

                    //OP<<"getCalledFunction :"<<CI->getCalledFunction()<<": "<<CI->getCalledFunction()->getName().str()<<"\n";
                    OP << "check func: " << fName << ": " << Func << "\n";
                    OOBCheckFuncs(I, Func);
                }
                else
                {
                    for (auto Func : Ctx->Callees[CI])
                    {
                        StringRef fName = Func->getName();
                        OP << "possible function: " << fName.str() << "\n";
                        OOBCheckFuncs(I, Func);
                    }
                }
                break;
            }
            default:{
                
            }
        }
        
    }
}
void FuncAnalysis::OOBCheckFuncs(llvm::Instruction *I, llvm::Function *Callee) {
    CallInst *CI = dyn_cast<CallInst>(I);
    if (Ctx->OOBFSummaries.find(Callee) == Ctx->OOBFSummaries.end() || Ctx->OOBFSummaries[Callee].isEmpty()){
        OP<<"no summary.\n";
        return;
    }
    for (int argNo = 0; argNo < CI->getNumArgOperands(); argNo++) {
        NodeIndex argNode = nodeFactory.getValueNodeFor(CI->getArgOperand(argNo));
        NodeIndex sumArgNode = Ctx->OOBFSummaries[Callee].args[argNo + 1].getNodeIndex();
        if (Ctx->OOBFSummaries[Callee].reqVec.at(sumArgNode) == _UNTAINTED) {
            for (auto item : relatedNode[argNode]) {
                for (auto aa : nAAMap[I][item]) {
                    //OP<<"aa = "<<aa<<"\n";
                    if (nodeFactory.isArgNode(aa)) {
                        OP<<"Function: "<<F->getName().str()<<"\n";
                        OP << "***********Warning: argument check fails @ argument "
                        <<argNo<<": variable is tainted.\n";
                        OP<<"Related BBs:\n";
                        std::set<std::string> whiteList = Ctx->OOBFSummaries[Callee].args[sumArgNode].getOOBWhiteList();
                        for (auto item : whiteList) {
                            OP<<item<<":";
                        }
                        OP<<"\n";
			printOOBRelatedBB(argNode, I, argNo, -1, Callee, whiteList);
                    }
                }
            }
        }
        OP<<"Value check done.\n";
        unsigned checkSize = 0;
        if (CI->getArgOperand(argNo)->getType()->isPointerTy())
        {
            const Type *type = cast<PointerType>(CI->getArgOperand(argNo)->getType())->getElementType();
            /// An array is considered a single variable of its type.
            while (const ArrayType *arrayType = dyn_cast<ArrayType>(type))
                type = arrayType->getElementType();
            if (const StructType *structType = dyn_cast<StructType>(type))
            {
                if (structType->isOpaque())
                {
                }
                else if (!structType->isLiteral() && structType->getStructName().startswith("union"))
                {
                    checkSize = 1;
                }
                else
                {
                    const StructInfo *stInfo = Ctx->structAnalyzer.getStructInfo(structType, M);
                    checkSize = stInfo->getExpandedSize();
                }
            }
        }
        for (auto sumObj : Ctx->OOBFSummaries[Callee].sumPtsGraph[sumArgNode]) {
            unsigned sumObjSize = Ctx->OOBFSummaries[Callee].args[argNo].getObjSize();
            unsigned sumObjOffset = Ctx->OOBFSummaries[Callee].args[argNo].getOffset();

            for (auto obj : nPtsGraph[I][argNode]) {
                if (obj < nodeFactory.getConstantIntNode())
                    continue;

                unsigned objSize = nodeFactory.getObjectSize(obj);
                //1. check the obj -> end of the obj
                unsigned objOffset = nodeFactory.getObjectOffset(obj);
                checkSize = std::min(checkSize, sumObjSize);
                checkSize = std::min(checkSize, objSize);
                for (unsigned i = 0; i < checkSize; i++) {
                    if (nodeFactory.isUnOrArrObjNode(obj)) {
                        if (Ctx->OOBFSummaries[Callee].reqVec.at(sumObj) == _UNTAINTED) {
                            for (auto item : relatedNode[obj]) {
                                for (auto aa : nAAMap[I][item]) {
                                    //OP<<"aa = "<<aa<<"\n";
                                    if (nodeFactory.isArgNode(aa)) {
                                        OP<<"Function: "<<F->getName().str()<<"\n";
                                        OP << "***********Warning: argument check fails @ argument "
                                            <<argNo<<": variable is tainted as oob\n";
                                        OP<<"Related BBs:\n";
                                        std::set<std::string> whiteList = Ctx->OOBFSummaries[Callee].args[sumArgNode].getOOBWhiteList();
                                        for (auto item : whiteList) {
                                            OP<<item<<":";
                                        }
                                        OP<<"\n";
                                        printOOBRelatedBB(obj, I, argNo, -1, Callee, whiteList);
                                    }
                                }       
                            }
                        }
                    }
                    if (obj + i >= nodeFactory.getNumNodes())
                        continue;

                    if (Ctx->OOBFSummaries[Callee].reqVec.at(sumObj + i) == _UNTAINTED) {
                        for (auto aa : nAAMap[I][obj+i]) {
                            if (nodeFactory.isArgNode(aa)) {
                                OP<<"Function: "<<F->getName().str()<<"\n";
                                OP << "***********Warning: argument check fails @ argument "
                                    <<argNo<<": variable is tainted as oob, field = "<<i<<"\n";
                                OP<<"Related BBs:\n";
                                std::set<std::string> whiteList = Ctx->OOBFSummaries[Callee].args[sumObj+i].getOOBWhiteList();
                                for (auto item : whiteList) {
                                    OP<<item<<":";
                                }
                                OP<<"\n";
                                printOOBRelatedBB(obj+i, I, argNo, i, Callee, whiteList);
                            }
                        }       

                        for (auto item : relatedNode[obj+i]) {
                            //OP<<"1item = "<<item<<"\n";
                            for (auto aa : nAAMap[I][item]) {
                                //OP<<"aa = "<<aa<<"\n";
                                if (nodeFactory.isArgNode(aa)) {
                                    OP<<"Function: "<<F->getName().str()<<"\n";
                                    OP << "***********Warning: argument check fails @ argument "
                                        <<argNo<<": variable is tainted as oob, field = "<<i<<"\n";
                                    OP<<"Related BBs:\n";
                                    std::set<std::string> whiteList = Ctx->OOBFSummaries[Callee].args[sumObj+i].getOOBWhiteList();
                                    for (auto item : whiteList) {
                                        OP<<item<<":";
                                    }
                                    OP<<"\n";
                                    printOOBRelatedBB(obj+i, I, argNo, i, Callee, whiteList);
                                }
                            }       
                        }
                    }
                }
                //2. if offset != 0
                if (objOffset > 0 && sumObjOffset > 0)
                {
                    for (unsigned i = 0; i < objOffset; i++)
                    {
                        if (Ctx->OOBFSummaries[Callee].reqVec.at(sumObj - i) == _UNTAINTED)
                        {
                            OP<<"Function: "<<F->getName().str()<<"\n";
                            OP << "***********Warning: argument check fails @ argument "
                                   <<argNo<<": variable is tainted as oob: field " << objOffset - i<<", nodeIndex = " << obj - i << "\n";
                            OP << "[trace] In function @" << F->getName().str() << " Instruction:" << *I << "\n";
                            OP<<"Related BBs:\n";
                            std::set<std::string> whiteList = Ctx->OOBFSummaries[Callee].args[sumArgNode].getOOBWhiteList();
                            for (auto item : whiteList) {
                                OP<<item<<":";
                            }
                            OP<<"\n";
                            printOOBRelatedBB(obj-i, I, argNo, i, Callee, whiteList);
                        }
                    }
                }
            }
        }

    }
}

void FuncAnalysis::printOOBRelatedBB(NodeIndex nodeIndex, const llvm::Value *Val,
                                    int argNo, int field, llvm::Function *Callee,
                                    std::set<std::string> &whiteList) {

    OP<<"Inside printIORelatedBB:\n";
    const llvm::Instruction *I = dyn_cast<const llvm::Instruction>(Val);

    if (!I) {
        OP << "Warning but no inst.\n";
        return;
    }

    std::set<const BasicBlock *> whitelist;
    std::set<NodeIndex> visit;

    BasicBlock *entry = &(F->front());
    json11::Json::array whiteArr;
    //json11::Json::array blackArr;
    //json11::Json::array altBlackArr;
    //DominatorTree T(*F);
    //std::set<const BasicBlock *> newBlacklist = blacklist;

    const llvm::BasicBlock *entryBB = &(F->front());

    for (auto witem : whitelist)
    {
        if ((I->getParent() != entryBB) && (witem == I->getParent()))
            continue;
        //OP<<"---whitelist: "<<witem->getName().str()<<"\n";
        whiteArr.push_back(json11::Json(witem->getName().str()));
    }

    //get the list of the node
    for (auto aa : nAAMap[I][nodeIndex])
    {
        for (auto item : nodeFactory.getOOBWL(aa))
        {
            whiteArr.push_back(json11::Json(item));
        }
    }
    //get the list for the argument
    if (Callee != NULL)
    {
        int sumArgNo = argNo + 1;
        for (auto sumObj : Ctx->OOBFSummaries[Callee].sumPtsGraph[sumArgNo])
        {
            int sumArgIdx = sumObj + field;
            for (auto witem : Ctx->OOBFSummaries[Callee].args[sumArgIdx].getWhiteList())
            {
                whiteArr.push_back(json11::Json(witem));
            }
        }
    }
    std::string moduleName = Ctx->funcToMd[I->getParent()->getParent()].str();
    //int start = targetDir.size();
    //std::string path = moduleName.substr(start);
    std::string insStr;
    llvm::raw_string_ostream rso(insStr);
    I->print(rso);
    //std::string id = moduleName+"_"+I->getParent()->getParent()->getName().str()+"_"+insStr+"_"+std::to_string(argNo)+"_"+std::to_string(field);
    std::string id = nodeFactory.getNodeName(nodeIndex);

    if (moduleName.size() > Ctx->absPath.size())
        moduleName = moduleName.substr(Ctx->absPath.size(), moduleName.size());
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

    json11::Json jsonObj = json11::Json::object{
        {"id", id},
        {"bc", moduleName},
        {"detector", "OOB"},
        {"oobWhitelist", json11::Json(whiteArr)},
        {"use", I->getParent()->getName().str()},
        {"function", I->getParent()->getParent()->getName().str()},
        {"warning", insStr},
        {"argNo", argNo},
        {"fieldNo", field},
        {"lineNo", itostr(line)},
        {"colNo", itostr(col)},
    };
    std::string json_str = jsonObj.dump();
    for (auto item : Ctx->OOBFSummaries[F].relatedBC)
    {
        OP << item << ":";
    }
    OP << "\n";
    OP << "json obj:" << json_str << "\n";

    std::string bcstr = "";
    for (auto item : Ctx->OOBFSummaries[F].relatedBC)
    {
        bcstr += item;
        bcstr += ":";
    }
    
    if (printWarning == false) {
        //OP<<"insert ("<<bcstr<<" \n";
        //OP<<json_str<<")\n";
        Ctx->fToOOBWarns[F].insert(std::make_pair(bcstr, json_str));
        return;
    }
#ifdef WRITEJSON
    std::ofstream jfile;
    if (Ctx->incAnalysis){
        jfile.open(Ctx->oobJsonIncFile, std::ios::app);
    }
    else {
        jfile.open(Ctx->oobJsonFile, std::ios::app);
    }
        
    //print the related bc files:
    jfile << bcstr << "\n";
    jfile << json_str << "\n";
    jfile.close();
#endif
}
