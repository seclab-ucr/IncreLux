#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/PatternMatch.h"

#include "llvm/IR/CFG.h"

#include <queue>
#include <deque>
#include <cstring>
#include <set>
#include <stack>
#include <utility>

#include "Annotation.h"
#include "QualifierAnalysis.h"
#include "FunctionSummary.h"
#include "CallGraph.h"
#include "Helper.h"

using namespace llvm;

//#define _PRINT_IOSUMNODEFACT
//#define _PRINT_INIT_ARRAY
//#define DEBUG_TITLE
//#define LIST_PROP
void FuncAnalysis::initIOSummary() {
    iofSummary.relatedBC.insert(F->getParent()->getName().str());
    //retValue and argValue node are created in: createInitNodes
    Instruction *beginIns = &(F->front().front());
    Instruction *endIns = &(F->back().back());
    //1-1. create return value node and argument value node
    NodeIndex sumRetNode = iofSummary.sumNodeFactory.getValueNodeFor(F);
    NodeIndex retNode = nodeFactory.getReturnNodeFor(F);
    //OP<<"return type: "<<*F->getReturnType()<<"\n";
    //1-2 create return object node
    if (F->getReturnType()->isVoidTy()) {
        iofSummary.sumPtsGraph[sumRetNode].clear();
    } else {
        //create node and build the pts relations of return object
        Type *type = F->getReturnType();
        Type *eleType = NULL;
        unsigned objSize = 0;
        if (!type->isVoidTy()) {
            if (type->isPointerTy()) {
                eleType = cast<PointerType>(type)->getElementType();
                while (const ArrayType *arrayType = dyn_cast<ArrayType>(eleType))
                    eleType = arrayType->getElementType();
                if (const StructType *structType = dyn_cast<StructType>(eleType)) {
                    if (structType->isOpaque()) {
                        objSize = 1;
                    } else if (!structType->isLiteral() && structType->getStructName().startswith("union")) {
                        objSize = 1;
                    } else {
                        const StructInfo *stInfo = Ctx->structAnalyzer.getStructInfo(structType, M);
                        objSize = stInfo->getExpandedSize();
                    }
                } else {
                    objSize = 1;
                }
            }
            else {
            }
        }
        iofSummary.setRetSize(objSize);
        for (auto obj : nPtsGraph[endIns][retNode]) {
            //OP << "obj = " << obj << "\n";
            if (obj <= nodeFactory.getConstantIntNode())
                continue;
            if (iofSummary.sumNodeFactory.getObjectNodeFor(F) != AndersNodeFactory::InvalidIndex)
                break;
	    NodeIndex sumRetObjNode = iofSummary.sumNodeFactory.createObjectNode(F);
            //OP<<"sumRetNode = "<<sumRetNode<<", objSize = "<<objSize<<"\n";
            if (nodeFactory.getObjectOffset(obj) == 0) {
                iofSummary.sumPtsGraph[sumRetNode].insert(sumRetObjNode);
                iofSummary.setRetOffset(0);
            }
            for (unsigned i = 1; i < objSize; i++) {
                NodeIndex sumObjNode = iofSummary.sumNodeFactory.createObjectNode(sumRetObjNode, i);
                if (i == nodeFactory.getObjectOffset(obj)) {
                    iofSummary.sumPtsGraph[sumRetNode].insert(sumObjNode);
                    iofSummary.setRetOffset(i);
                }
            }
        }
    }
    //OP<<"create object node for args.\n";
    //1-3 create argument object node
    int argNo = 0;
    for (auto const &a : F->args()) {
        const Argument *arg = &a;
        NodeIndex argNode = nodeFactory.getValueNodeFor(arg);
        NodeIndex sumArgNode = iofSummary.sumNodeFactory.getValueNodeFor(arg);
        //OP<<"argNo = "<<argNo<<", sumArgNode = "<<sumArgNode<<"\n";

        iofSummary.args[sumArgNode].setNodeIndex(sumArgNode);
        iofSummary.args[sumArgNode].setNodeArgNo(argNo);
        //OP<<"sumArgNode = "<<sumArgNode<<", argNode = "<<argNode<<"\n";;
        //create node for object, set the sumPts information
        //create the array for update and requirement of the function
        for (auto obj : nPtsGraph[endIns][argNode]) {
            //OP<<"obj = "<<obj<<"\n";
            if (obj <= nodeFactory.getConstantIntNode())
                continue;
            unsigned objSize = nodeFactory.getObjectSize(obj);
            NodeIndex sumArgObjNode = iofSummary.sumNodeFactory.getObjectNodeFor(arg);
            //OP<<"obj index = "<<iofSummary.sumNodeFactory.getObjectNodeFor(arg)<<", objSize = "<<objSize<<"\n";;
            if (sumArgObjNode != AndersNodeFactory::InvalidIndex)
                break;
            iofSummary.args[argNo].setObjSize(objSize);
            sumArgObjNode = iofSummary.sumNodeFactory.createObjectNode(arg);
            //OP<<"sumArgObjNode = "<<sumArgObjNode<<"\n";
            if (nodeFactory.getObjectOffset(obj) == 0) {
                iofSummary.sumPtsGraph[sumArgNode].insert(sumArgObjNode);
                iofSummary.args[sumArgObjNode].setNodeArgNo(argNo);
                iofSummary.args[sumArgObjNode].setOffset(0);
            }
            for (unsigned i = 1; i < objSize; i++) {
                NodeIndex objNode = iofSummary.sumNodeFactory.createObjectNode(sumArgObjNode, i);
                iofSummary.args[objNode].setNodeArgNo(argNo);
                if (i == nodeFactory.getObjectOffset(obj)) {
                    iofSummary.sumPtsGraph[sumArgNode].insert(objNode);
                    iofSummary.args[argNo].setOffset(i);
                }
            }
        }
        argNo++;
    }
#ifdef _PRINT_IOSUMNODEFACT
    OP << "sumNodeFactory:\n";
    for (int i = 0; i < iofSummary.sumNodeFactory.getNumNodes(); i++)
        iofSummary.sumNodeFactory.dumpNode(i);
    OP << "sumPtsGraph:\n";
    for (auto i : iofSummary.sumPtsGraph)
    {
        OP << "ptr: " << i.first << "->";
        if (i.second.size() >0) {
            for (auto o : i.second) {
                OP << o << "\n"; 
            }
        }
    }
#endif
    //2. create the summary array for both update and requirments
    unsigned numNodes = iofSummary.sumNodeFactory.getNumNodes();
    iofSummary.noNodes = numNodes;
    iofSummary.reqVec.resize(numNodes);    //= new int[numNodes];
    iofSummary.updateVec.resize(numNodes); //= new int[numNodes];
    for (unsigned i = 0; i < numNodes; i++) {
        iofSummary.reqVec.at(i) = _UNKNOWN;
        iofSummary.updateVec.at(i) = _UNKNOWN;
        //iofSummary.changeVec.at(i) = _UNKNOWN;
    }
}

void FuncAnalysis::ioQualiInference() {
//dbg information
#ifdef DEBUG_TITLE
    OP << "Inside qualifier inference for F :"<<F->getName().str()<<", addr : "<<F<<"\n";
#endif
    //The length of the qualifier array, the total number of the nodes
    const unsigned numNodes = nodeFactory.getNumNodes();
    std::vector<int> initArray;
    ioQualiReq.resize(numNodes);
    initArray.resize(numNodes);
    VisitIns.clear();
    //set all the init qualifier of local variable as UNKNOWN,
    //because the merge may happen before the variable really appears in the program;
    //Ex:%11 = phi %1, %2, but %1 and %2 comes from different block let the %11 always be _UD;
    for (NodeIndex i = 0; i < numNodes; i++) {
        ioQualiReq.at(i) = _UNKNOWN;
        initArray.at(i) = _UNKNOWN;
    }
    Instruction *entry = &(F->front().front());
    NodeIndex entryIndex = nodeFactory.getValueNodeFor(entry);
    ReturnInst *RI = NULL;
    ioSetGlobalQualies(initArray);

#ifdef _PRINT_INIT_ARRAY
    OP << "init array:\n";
    for (int i = 0; i < numNodes; i++)
    {
        OP << "node " << i << ", qualifier = " << initArray[i] << "\n";
    }
#endif
    for (inst_iterator itr = inst_begin(*F), ite = inst_end(*F); itr != ite; ++itr) {
        auto inst = itr.getInstructionIterator();
        llvm::Instruction *I = &*inst;
        if (isa<ReturnInst>(I)) {
            RI = dyn_cast<ReturnInst>(I);
        }
        switch (I->getOpcode()) {
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
            case Instruction::Shl: {
		NodeIndex dstIndex = nodeFactory.getValueNodeFor(I);
                NodeIndex op0Index = nodeFactory.getValueNodeFor(I->getOperand(0));
                NodeIndex op1Index = nodeFactory.getValueNodeFor(I->getOperand(1));
		if (op0Index > nodeFactory.getConstantIntNode()) {
                    ioQualiReq.at(op0Index) = _UNTAINTED;
                    for (auto aa:nAAMap[I][op0Index]) {
			for (auto relatedObj: relatedNode[aa]) {
			    if (nodeFactory.isArgNode(relatedObj)) {
		  	        ioQualiReq.at(relatedObj) = _UNTAINTED;
                                nodeFactory.addToIOWL(relatedObj, I->getParent()->getName().str());
			    }
			}
                    }
                }
                if (op1Index > nodeFactory.getConstantIntNode()) {
                    ioQualiReq.at(op1Index) = _UNTAINTED;
                    for (auto aa:nAAMap[I][op1Index]) {
		        for (auto relatedObj: relatedNode[aa]) {
			    if (nodeFactory.isArgNode(relatedObj)) {
                                ioQualiReq.at(relatedObj) = _UNTAINTED;
                                nodeFactory.addToIOWL(relatedObj, I->getParent()->getName().str());
			    }
                        }
                    }
                }
                break;
            }
            case Instruction::Call: {
                CallInst *CI = dyn_cast<CallInst>(I);
		/*do nothing about dbgInfoIntrinsic*/
                if (isa<DbgInfoIntrinsic>(I)) {
                    break;
                }
                if (CI->getCalledFunction()) {
                    llvm::Function *Callee = CI->getCalledFunction();
		    std::string fName = Callee->getName().str();
                    if (Ctx->CopyFuncs.count(fName)) {
                        //OP << "process memcpy funcs\n";
                        processIOCopyFuncs(I, Callee);
                    }
                    else{
                        ioProcessFuncs(I, Callee);
                    }
                } else {
                    for (FuncSet::iterator it = Ctx->Callees[CI].begin(), ite = Ctx->Callees[CI].end();
                         it != ite; it++) {
                        llvm::Function *Callee = *it;
                    	std::string fName = Callee->getName().str();
                   	 if (Ctx->CopyFuncs.count(fName)) {
                            //OP << "process memcpy funcs\n";
                            processIOCopyFuncs(I, Callee);
                        }
                        else{
                            ioProcessFuncs(I, Callee);
                        }
		    }
                }
            }
        }
        /*for (int i = 0; i < numNodes; i++) {
            OP<<i<<"/";
        }
        OP<<"\n";
        for (int i = 0; i < numNodes; i++) {
            OP<<ioQualiReq[i]<<"/";
        }
        OP<<"\n";*/
    }//switch(I->getOpCode())
    if (!F->getReturnType()->isVoidTy() && RI) {
	NodeIndex retNode = nodeFactory.getValueNodeFor(RI->getReturnValue());
        NodeIndex sumRetNode = iofSummary.sumNodeFactory.getValueNodeFor(F);
	for (auto item : relatedNode[retNode])
        {
            //OP<<"item : "<<item<<", ";
            for (auto aa : nAAMap[RI][item])
            {
                //OP<<"aa = "<<aa<<"\n";
                const llvm::Value *val = nodeFactory.getValueForNode(aa);
                NodeIndex idx = AndersNodeFactory::InvalidIndex;
                int offset = 0;
                if (nodeFactory.isObjectNode(aa))
                {
                    offset = nodeFactory.getObjectOffset(aa);
                    NodeIndex valIdx = iofSummary.sumNodeFactory.getValueNodeFor(val);
                    if (valIdx == AndersNodeFactory::InvalidIndex)
                        continue;
                    for (auto obj : iofSummary.sumPtsGraph[valIdx])
                    {
                        idx = obj;
                        break;
                    }
                }
                else
                {
                    idx = iofSummary.sumNodeFactory.getValueNodeFor(val);
                }
                    //const llvm::Value *val = nodeFactory.getValueForNode(item);
                    //OP<<"Value : "<<*val<<", ";
                    //OP<<"idx = "<<idx<<", offset = "<<offset<<"\n";
                if (idx != AndersNodeFactory::InvalidIndex)
                {
                        //OP<<"idx + offset = "<<idx+offset<<"\n";
                    iofSummary.args[sumRetNode].addToRelatedArg(idx + offset);
                }
            }
        }
	bool init = false;
	for (auto sumObj : iofSummary.sumPtsGraph[sumRetNode]) {
	    unsigned sumObjOffset = iofSummary.sumNodeFactory.getObjectOffset(sumObj);
            unsigned sumObjSize = iofSummary.sumNodeFactory.getObjectSize(sumObj);
	    for (auto obj : nPtsGraph[RI][retNode]) {
		if (!init)
                {   
                    for (unsigned i = 0; i < sumObjSize; i++)
                    {   
                        if (obj - sumObjOffset + i >= nodeFactory.getNumNodes())
                            break; 
                        else
                        {
                            const llvm::Value *val = nodeFactory.getValueForNode(obj - sumObjOffset + i);
                            NodeIndex idx = iofSummary.sumNodeFactory.getValueNodeFor(val);
                            if (idx != AndersNodeFactory::InvalidIndex) {
                                for (auto relatedObj : iofSummary.sumPtsGraph[idx]) {
                                    iofSummary.args[sumObj - sumObjOffset + i].addToRelatedArg(relatedObj-sumObjOffset + i);
                                }
                            }
                        }
                    }
                    init = true;
                }
		else
                {
                    for (unsigned i = 0; i < sumObjSize; i++)
                    {
                        //Tobe Fix: there should be a way to avoid this: function get_ringbuf
                        if (obj - sumObjOffset + i >= nodeFactory.getNumNodes())
                            break;
                        else
                        {
                            const llvm::Value *val = nodeFactory.getValueForNode(obj - sumObjOffset + i);
                            NodeIndex idx = iofSummary.sumNodeFactory.getValueNodeFor(val);
                            if (idx != AndersNodeFactory::InvalidIndex)
                            {
                                for (auto relatedObj : iofSummary.sumPtsGraph[idx]) {
                                    iofSummary.args[sumObj - sumObjOffset + i].addToRelatedArg(relatedObj-sumObjOffset + i);
                                }
                            }
                        }
                    }
                }
	    }
        }
    }
    ioSummarizeFuncs(RI);
}


void FuncAnalysis::ioProcessFuncs(llvm::Instruction *I, llvm::Function *Callee) {
    CallInst *CI = dyn_cast<CallInst>(I);
    std::vector<const Argument *> calleeArgs;
    for (Function::arg_iterator itr = Callee->arg_begin(), ite = Callee->arg_end(); itr != ite; ++itr) {
        const Argument *arg = &*itr;
        calleeArgs.push_back(arg);
    }

    if (Callee->empty())
    {
        auto FIter = Ctx->Funcs.find(Callee->getName().str());
        if (FIter != Ctx->Funcs.end())
        {
            Callee = FIter->second;
        }
    }
    int sumNumNodes = Ctx->FSummaries[Callee].noNodes;
    unsigned numNodes = nodeFactory.getNumNodes();
	
    if (Ctx->incAnalysis && Ctx->IOFSummaries.find(Callee) == Ctx->IOFSummaries.end())
    {
        //assert(Ctx->modifiedFuncs.find(Callee) == Ctx->modifiedFuncs.end() && "modifiedFuncs not summarized before being called.");
        std::string fname = getScopeName(Callee, Ctx->funcToMd, Ctx->Funcs);

        size_t pos = fname.find(Ctx->newVersion);
        if (pos != std::string::npos)
        {
            fname.replace(pos, Ctx->newVersion.size(), Ctx->oldVersion);
        }

        std::string sFile = Ctx->outFolder + "/IOSummary/" +fname + ".iosum";
        //OP << "[[callee's iosummary file :" << sFile << "\n";
        std::ifstream ifile(sFile);
        if (ifile)
        {
            //std::ifstream ifile(sFile);
            boost::archive::text_iarchive ia(ifile);
            ia >> Ctx->IOFSummaries[Callee];
            //Ctx->FSummaries[Callee].summary();
            ifile.close();
            OP << "[[Summary for  " << fname << " loaded!\n";
        }
        else {
	    OP << "[[callee's iosummary file :" << sFile << "\n";
            OP<<"[[Summary not existed.\n";
        }
    }

    if (Ctx->IOFSummaries[Callee].isEmpty()) {
	//OP << "Summary is empty\n";
	//Ctx->IOFSummaries[Callee].summary();
        return;
    }
    for (int argNo = 0; argNo < CI->getNumArgOperands(); argNo++) {
        NodeIndex argNode = nodeFactory.getValueNodeFor(CI->getArgOperand(argNo));
        NodeIndex sumArgNode = Ctx->IOFSummaries[Callee].args[argNo + 1].getNodeIndex();
	//OP<<"argNode = "<<argNo<<", sumArgNode = "<<sumArgNode<<", Ctx->IOFSummaries[Callee].req = "<<Ctx->IOFSummaries[Callee].reqVec.at(sumArgNode)<<"\n";
        if (Ctx->IOFSummaries[Callee].reqVec.at(sumArgNode) == _UNTAINTED) {
            for (auto aa : nAAMap[I][argNode]) {
                if (nodeFactory.isArgNode(aa)) {
                    ioQualiReq.at(aa) = _UNTAINTED;
#ifdef LIST_PROP
                    std::set<std::string> ioWL = Ctx->IOFSummaries[Callee].args[sumArgNode].getIOWhiteList();

                    for (auto item : ioWL) {
                        nodeFactory.addToIOWL(aa, item);
                    }
#endif
                }
            }
        }
        for (auto sumObj : Ctx->IOFSummaries[Callee].sumPtsGraph[sumArgNode]) {
            unsigned sumObjSize = Ctx->IOFSummaries[Callee].args[argNo].getObjSize();
            unsigned sumObjOffset = Ctx->IOFSummaries[Callee].args[argNo].getOffset();
            for (auto obj : nPtsGraph[I][argNode])
            {
                if (obj <= nodeFactory.getConstantIntNode())
                    continue;
                //Deal with the union object
                unsigned objSize = nodeFactory.getObjectSize(obj);
                //OP<<"2end'\n";
                unsigned objOffset = nodeFactory.getObjectOffset(obj);
                unsigned checkSize = std::min(objSize - objOffset, sumObjSize - sumObjOffset);
                if (nodeFactory.isUnOrArrObjNode(obj))
                {
                    bool init = false;
                    //OP<<"obj "<<obj<<" is unionObj.\n";
                    for (unsigned i = 0; i < checkSize; i++)
                    {
                        //OP<<"i = "<<i<<", sumObj + i = "<<sumObj+i<<"\n";
                        //OP<<"update :"<<Ctx->FSummaries[Callee].updateVec[sumObj + i]<<"\n";
                        //OP<<"before update, the qualifier of obj + i = "<<out[obj+i]<<"\n";
                        if (Ctx->IOFSummaries[Callee].reqVec[sumObj + i] == _UNTAINTED)
                        {
                            for (auto aa : nAAMap[I][obj]) {
                                ioQualiReq.at(obj) = _UNTAINTED;
#ifdef LIST_PROP
                                std::set<std::string> ioWL = Ctx->IOFSummaries[Callee].args[sumObj+i].getIOWhiteList();
                                for (auto item : ioWL) {
                                    nodeFactory.addToIOWL(obj, item);
                                }
#endif
                            }
                        }
                    }
                }
                else
                {
                    //OP<<"Not union.\n";
                    //update the arg->obj from the offset till end
                    for (unsigned i = 0; i < checkSize; i++)
                    {
                        //OP<<"i = "<<i<<", sumobj + i = "<<sumObj+i<<". obj + i = "<<obj+i<<"\n";
                        if (obj + i >= numNodes)
                            break;

                        if (Ctx->IOFSummaries[Callee].reqVec[sumObj+i] == _UNTAINTED) {
                            for (auto aa : nAAMap[I][obj+i]) {
                                ioQualiReq.at(obj+i) = _UNTAINTED;
#ifdef LIST_PROP
                                std::set<std::string> ioWL = Ctx->IOFSummaries[Callee].args[sumObj+i].getIOWhiteList();
                                for (auto item : ioWL) {
                                    nodeFactory.addToIOWL(obj+i, item);
                                }
#endif
                            }
                        }
                    }
                }
                //update the rest object if arg point to the middle of the obj
                //unsigned objOffset = nodeFactory.getObjectOffset(obj);
                if (sumObjOffset > 0 && objOffset > 0)
                {
                    if (sumObjOffset < objOffset)
                        continue;

                    //unsigned objSize = nodeFactory.getObjectSize(obj);
                    for (unsigned i = objOffset; i > 0; i--)
                    {
                        if (Ctx->IOFSummaries[Callee].reqVec[sumObj - i] == _UNTAINTED)
                        {
                            for (auto aa : nAAMap[I][obj-i]) {
                                ioQualiReq.at(obj-i) = _UNTAINTED;
#ifdef LIST_PROP
                                std::set<std::string> ioWL = Ctx->IOFSummaries[Callee].args[sumObj-i].getIOWhiteList();
                                for (auto item : ioWL) {
                                    nodeFactory.addToIOWL(obj-i, item);
                                }
#endif
                            }
                        }
                    }
                }
            }
        }//sumPtsGraph
    }
    NodeIndex retNode = nodeFactory.getValueNodeFor(I);
    NodeIndex sumRetNode = Ctx->FSummaries[Callee].getRetNodes();
    std::set<NodeIndex> sumRSet = Ctx->FSummaries[Callee].args[sumRetNode].getRelatedArgs();
    if (!sumRSet.empty()) {
	for (auto sumRitem : sumRSet) {
	    int argNoCurr = Ctx->FSummaries[Callee].args[sumRitem].getNodeArgNo();
            if (argNoCurr >= CI->getNumArgOperands() || isa<Function>(CI->getArgOperand(argNoCurr)))
                continue;
            //OP<<"argNoCurr2 = "<<argNoCurr<<", ArgOprand: "<<*CI->getArgOperand(argNoCurr)<<"\n";
            NodeIndex argNoIndex = nodeFactory.getObjectNodeFor(CI->getArgOperand(argNoCurr));
            int argOffset = Ctx->FSummaries[Callee].args[sumRitem].getOffset();
	    if (argNoIndex == AndersNodeFactory::InvalidIndex && argOffset == 0)
            {
                argNoIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(argNoCurr));
                //OP<<"2argNoIndex = "<<argNoIndex<<"\n";
            }
            if (argNoIndex == AndersNodeFactory::InvalidIndex)
                continue;
	    relatedNode[retNode].insert(relatedNode[argNoIndex + argOffset].begin(), relatedNode[argNoIndex + argOffset].end());
	}
    }//if (!sumRSet.empty())


    for (auto sumRetObj : Ctx->FSummaries[Callee].sumPtsGraph[sumRetNode]) {
	unsigned sumRetSize = Ctx->FSummaries[Callee].getRetSize();
        unsigned sumRetOffset = Ctx->FSummaries[Callee].getRetOffset();
	for (auto retObj : nPtsGraph[I][retNode]) {
	    if (retObj <= nodeFactory.getConstantIntNode())
                continue;
            unsigned retObjSize = nodeFactory.getObjectSize(retObj);
            unsigned retObjOffset = nodeFactory.getObjectOffset(retObj);
            unsigned checkSize = std::min(sumRetSize - sumRetOffset, retObjSize - retObjOffset);

	    for (unsigned i = 0; i < checkSize; i++) {
		std::set<NodeIndex> sumObjRSet = Ctx->FSummaries[Callee].args[sumRetObj+i].getRelatedArgs();
		for (auto sumRitem : sumObjRSet) {
		    int argNoCurr = Ctx->FSummaries[Callee].args[sumRitem].getNodeArgNo();
		    NodeIndex argNoIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(argNoCurr));
            	    int argOffset = Ctx->FSummaries[Callee].args[sumRitem].getOffset();
		    if (argNoCurr >= CI->getNumArgOperands() || isa<Function>(CI->getArgOperand(argNoCurr)))
                	continue;
		    for (auto argObj : nPtsGraph[I][argNoIndex]) {
			relatedNode[retObj + i].insert(argObj+i);
			relatedNode[retObj + i].insert(relatedNode[argObj + i].begin(), relatedNode[argObj + i].end());
		    }
		}
	    }
	}
    }
}

void FuncAnalysis::ioSummarizeFuncs(llvm::ReturnInst *RI) {
    Instruction *entry = &(F->front().front());
    Instruction *end = &(F->back().back());
    unsigned numNodes = nodeFactory.getNumNodes();
    std::set<const BasicBlock *> whitelist;

    for (auto &a : F->args()) {
        Argument *arg = &a;
        NodeIndex argIndex = nodeFactory.getValueNodeFor(arg);
        NodeIndex sumArgIndex = iofSummary.sumNodeFactory.getValueNodeFor(arg);
        //OP<<"arg: "<<*arg<<"\n";
	//OP<<"argIndex = "<<argIndex<<", sumArgIndex = "<<sumArgIndex<<"\n";
	if (ioQualiReq.at(argIndex) == _UNTAINTED) {
            iofSummary.reqVec.at(sumArgIndex) = _UNTAINTED;
            //void calculateIORelatedBB(NodeIndex , const llvm::Instruction *I, std::set<NodeIndex> &visit, 
            //                            std::set<const BasicBlock *> &blacklist, std::set<const BasicBlock *> &whitelist)
            std::set<std::string> WL = nodeFactory.getIOWL(argIndex);
            iofSummary.args[sumArgIndex].setIOWhiteList(WL);
        }


        NodeIndex sumArgObjIndex = iofSummary.sumNodeFactory.getObjectNodeFor(arg);
        if (sumArgObjIndex == AndersNodeFactory::InvalidIndex)
            continue;
        unsigned sumObjSize = iofSummary.sumNodeFactory.getObjectSize(sumArgObjIndex);
        unsigned sumObjOffset = iofSummary.sumNodeFactory.getObjectOffset(sumArgObjIndex);
        for (auto obj : nPtsGraph[entry][argIndex]) {
            if (obj <= nodeFactory.getConstantIntNode())
                continue;
            for (unsigned i = 0; i < sumObjSize; i++) {
                //OP<<"i = "<<i<<", obj - sumObjOffset + i ="<<obj - sumObjOffset +i<<"\n";
                //copy the requirement
                if (ioQualiReq.at(obj - sumObjOffset + i) == _UNTAINTED) {
                    //OP<<"sumArgObjIndex - sumObjOffset + i = "<<sumArgObjIndex - sumObjOffset + i<<"\n";
                    iofSummary.reqVec.at(sumArgObjIndex - sumObjOffset + i) = _UNTAINTED;
                    std::set<std::string> WL = nodeFactory.getIOWL(obj - sumObjOffset + i);
                    iofSummary.args[sumArgObjIndex - sumObjOffset + i].setIOWhiteList(WL);
                }
            }
        }
    } //F->args()
#ifdef _PRINT_IOSUM
    OP<<"print iofSummary.summary():\n";
    iofSummary.summary();
#endif
}

void FuncAnalysis::ioSetGlobalQualies(std::vector<int> &initArray) {
    //globalVars|globalVars|FunctionNode|argNode|localVar
    //Set things before argNodes as initialized
    Instruction *entry = &(F->front().front());
    NodeIndex entryNode = nodeFactory.getValueNodeFor(entry);
    bool init = false;
    for (Function::arg_iterator itr = F->arg_begin(), ite = F->arg_end(); itr != ite; ++itr) {
        const Argument *arg = &*itr;
        NodeIndex valNode = nodeFactory.getValueNodeFor(arg);

        for (NodeIndex i = 0; i < valNode; i++) {
            initArray.at(i) = _UNTAINTED;
        }
        init = true;
        break;
    }
    //if the function does not have the args
    if (!init) {
        for (NodeIndex i = 0; i < entryNode; i++) {
            initArray.at(i) = _UNTAINTED;
        }
    }
    //set the qualifier of FunctionNode
    for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f) {
        Function *func = &*f;
        if (!func->hasAddressTaken())
            continue;
        NodeIndex fIndex = nodeFactory.getValueNodeFor(func);
        getDef(func);
        std::string fname = getScopeName(func, Ctx->funcToMd, Ctx->Funcs);
        //OP<<"fname = "<<fname<<"\n";
        if (Ctx->IOFSummaries.find(func) != Ctx->IOFSummaries.end() && Ctx->IOFSummaries[func].reqVec.size()) {
            //OP<<"size of updateVec: "<<Ctx->FSummaries[func].updateVec.size()<<"\n";
            //ret node is the first node (index 0) in the summary
            initArray.at(fIndex) = Ctx->IOFSummaries[func].reqVec.at(0);
        } else {
            initArray.at(fIndex) = _UNTAINTED;
        }
    }
    //set the qualifier of the argument obj and value to _UNKNOWN
    for (Function::arg_iterator itr = F->arg_begin(), ite = F->arg_end(); itr != ite; ++itr) {
        const Argument *arg = &*itr;
        //get the value node and set the initArray
        NodeIndex valNode = nodeFactory.getValueNodeFor(arg);
        initArray.at(valNode) = _UNKNOWN;

        NodeIndex objNode = nodeFactory.getObjectNodeFor(arg);
        if (objNode != AndersNodeFactory::InvalidIndex) {
            unsigned objSize = nodeFactory.getObjectSize(objNode);
            unsigned objOffset = nodeFactory.getObjectOffset(objNode);
            for (unsigned i = objNode - objOffset; i < objNode - objOffset + objSize; i++) {
                initArray.at(i) = _UNKNOWN;
            }
        }
    }
}

void FuncAnalysis::ioQualiJoin(std::vector<int> &qualiA, std::vector<int> &qualiB, unsigned len) {
    for (unsigned i = 0; i < len; i++) {
        //OP<<"i = "<<i<<", qualiA = "<<qualiA.at(i)<<", qualiB = "<<qualiB.at(i)<<"\n";
        if (qualiA.at(i) == _TAINTED || qualiB.at(i) == _TAINTED) {
            qualiA.at(i) = _TAINTED;
        } else if (qualiA.at(i) == _UNKNOWN || qualiB.at(i) == _UNKNOWN) {
            qualiA.at(i) = _UNKNOWN;
        } else {
            qualiA.at(i) = std::max(qualiA.at(i), qualiB.at(i));
        }
    }
}
void FuncAnalysis::calculateIORelatedBB(NodeIndex nodeIndex, const llvm::Instruction *I, std::set<const BasicBlock *> &whitelist) {
    const llvm::Value *value = nodeFactory.getValueForNode(nodeIndex);
}
void FuncAnalysis::processIOCopyFuncs(llvm::Instruction *I, llvm::Function *Callee) {
	    unsigned numNodes = nodeFactory.getNumNodes();
    //OP<<"numNodes = "<<nodeFactory.getNumNodes()<<"\n";
    CallInst *CI = dyn_cast<CallInst>(I);
    if (CI->getNumArgOperands() < 3)
        return;
        Value *dst = CI->getArgOperand(0);
    Value *src = CI->getArgOperand(1);
    Value *size = CI->getArgOperand(2);
    NodeIndex dstIndex = nodeFactory.getValueNodeFor(dst);
    NodeIndex srcIndex = nodeFactory.getValueNodeFor(src);
    NodeIndex sizeIndex = nodeFactory.getValueNodeFor(size);

    uint64_t objSize = 0;

    for (auto obj : nPtsGraph[I][dstIndex])
    {
        if (obj <= nodeFactory.getConstantIntNode())
            continue;
        if (objSize > 0)
            objSize = ((objSize < nodeFactory.getObjectSize(obj)) ? objSize : nodeFactory.getObjectSize(obj));
        else
            objSize = nodeFactory.getObjectSize(obj);
        if (nodeFactory.isUnOrArrObjNode(obj))
        {
            objSize = 1;
            break;
        }
        //copy from middle, the field pointer.
        if (objSize > nodeFactory.getObjectOffset(obj) && nodeFactory.getObjectOffset(obj) != 0)
        {
            objSize = nodeFactory.getObjectSize(obj) - nodeFactory.getObjectOffset(obj);
        }
    }
    for (auto srcObj : nPtsGraph[I][srcIndex])
    {
        if (srcObj <= nodeFactory.getConstantIntNode())
            continue;
        if (nodeFactory.isUnOrArrObjNode(srcObj))
        {
            objSize = 1;
            break;
        }
        unsigned srcObjOffset = nodeFactory.getObjectOffset(srcObj);
        objSize = ((objSize < nodeFactory.getObjectSize(srcObj) - srcObjOffset) ? objSize : nodeFactory.getObjectSize(srcObj) - srcObjOffset);
    }

    if (BitCastInst *BCI = dyn_cast<BitCastInst>(src))
    {
        const Type *type = cast<PointerType>(I->getOperand(0)->getType())->getElementType();

        while (const ArrayType *arrayType = dyn_cast<ArrayType>(type))
            type = arrayType->getElementType();
        if (const StructType *structType = dyn_cast<StructType>(type))
        {
            if (!structType->isOpaque())
            {
                const StructInfo *stInfo = Ctx->structAnalyzer.getStructInfo(structType, F->getParent());
                objSize = stInfo->getExpandedSize();
            }
            //OP<<"4objSize = "<<objSize<<"\n";
            if (!structType->isLiteral() && structType->getStructName().startswith("union"))
                objSize = 1;
        }
    }

    for (auto dstObj : nPtsGraph[I][dstIndex]) {
	for (auto srcObj : nPtsGraph[I][srcIndex]) {
	    for (int i = 0; i < objSize; i++) {
		if (ioQualiReq.at(srcObj+i) == _UNTAINTED) {
		    ioQualiReq.at(dstObj+i) = _UNTAINTED;
		}
	    }
        }
    }
}
