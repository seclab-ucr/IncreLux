#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>

#include <deque>

#include "Common.h"
#include "Annotation.h"
#include "PointTo.h"

//#define _INIT_PTS
//#define _PRINT_NODEFACTORY
//#define _PRINT_INST
#define DEBUG_TITLE
//#define _PRINT_PTS
//#define INOUT
//#define gep_check

using namespace llvm;

bool PointToAnalysis::run() {
    OP << "==========Function " << F->getName().str() << "==========\n";

    PtsGraph initPtsGraph;
    createInitNodes(initPtsGraph);

#ifdef _INIT_PTS
    OP << "init ptsGraph:\n";
    for (auto i:initPtsGraph) {
        OP << "index " << i.first << " point to <";
        for (auto it : i.second)
            OP << it << ", ";
        OP << ">\n";
    }

    /*for (int i = 0; i < nodeFactory.getNumNodes(); i++){
        nodeFactory.dumpNode(i);
    }*/
#endif

    // preprocessing:
    // 1. create node for all values
    for (inst_iterator itr = inst_begin(*F), ite = inst_end(*F); itr != ite; ++itr) {
        nodeFactory.createValueNode(&*itr);
    }

    std::deque<const BasicBlock*> worklist;
    for (auto const& bb : *F) {
        worklist.push_back(&bb);
    }

    unsigned count = 0;
    unsigned threshold = 20 * worklist.size();

    while (!worklist.empty() && ++count <= threshold) {
        const BasicBlock *BB = worklist.front();
        worklist.pop_front();

        PtsGraph in;
        PtsGraph out;

        // calculates BB in
        if (BB == &F->front()) {
            in = initPtsGraph;
        } else {
            for (auto pi = pred_begin(BB), pe = pred_end(BB); pi != pe; ++pi) {
                const BasicBlock *pred = *pi;
                const Instruction *b = &pred->back();
                unionPtsGraph(in, nPtsGraph[b]);
            }
        }

        bool changed = false;
        for (auto const &ii : *BB) {
            const Instruction *I = &ii;

            if (I != &BB->front())
                in = nPtsGraph[I->getPrevNode()];
            
#ifdef _PRINT_INST
            OP<<*I<<"\n";
#endif
#ifdef INOUT
            OP << "===========IN===========\n";
            for (auto i:in) {
                if(i.first < entryIndex && i.first>nodeFactory.getNullObjectNode())
                    continue;
                OP << "index " << i.first << " point to <";
                for (auto it : i.second)
                    OP << it << ", ";
                O P<< ">\n";
            }
#endif

            out = processInstruction(I, in);
            if (I == &BB->back()) {
                PtsGraph old = nPtsGraph[I];
                if (out != old)
                    changed = true;
            }
            nPtsGraph[I] = out;

#ifdef INOUT
            OP << "===========OUT===========\n";
            for (auto i:out) {
                if(i.first < entryIndex && i.first>nodeFactory.getNullObjectNode())
                    continue;
                if(i.second == in[i.first])
                    continue;
                OP << "index " << i.first << " point to <";
                for (auto it : i.second)
                    OP << it << ", ";
                OP << ">\n";
            }
#endif
        }

        if (changed) {
            for (auto si = succ_begin(BB), se = succ_end(BB); si != se; ++si) {
                const BasicBlock *succ = *si;
                if (std::find(worklist.begin(), worklist.end(), succ) == worklist.end())
                    worklist.push_back(succ);
            }
        }
    } //worklist

    if (count > threshold)
        OP << "WARNING: incomplete point-to analysis for " << F->getName() << "\n";

#ifdef _PRINT_NODEFACTORY
    OP << "NodeFactory:\n";
    for (int i = 0; i < nodeFactory.getNumNodes(); i++)
        nodeFactory.dumpNode(i);
#endif

#ifdef _PRINT_PTS
    OP << "The ptsGraph at the final node:\n";
    for (auto i:nPtsGraph[&F->back().back()]) {
        OP << "index " << i.first << " point to <";
        for (auto it : i.second)
            OP << it << ", ";
        OP << ">\n";
    }
#endif
    return true;
}

void PointToAnalysis::createNodeForPointerVal(const Value *V, const Type *T, const NodeIndex valNode, PtsGraph &ptsGraph) {

    if (!T->isPointerTy())
        return;

    assert(valNode != AndersNodeFactory::InvalidIndex);

    const Type *type = cast<PointerType>(T)->getElementType();

    // An array is considered a single variable of its type.
    while (const ArrayType *arrayType= dyn_cast<ArrayType>(type))
        type = arrayType->getElementType();

    // Now construct the pointer and memory object variable
    // It depends on whether the type of this variable is a struct or not
    if (const StructType *structType = dyn_cast<StructType>(type)) {
        processStruct(V, structType, valNode, ptsGraph);
    } else {
        NodeIndex obj = nodeFactory.createObjectNode(V);
        ptsGraph[valNode].insert(obj);
        // XXX: conservative assumption
        if (type->isPointerTy())
            ptsGraph[obj].insert(nodeFactory.getUniversalPtrNode());
    }
}

void PointToAnalysis::createInitNodes(PtsGraph &ptsGraph) {

    NodeIndex valNode;
    Type *type;

    assert(!F->isDeclaration() && !F->isIntrinsic());

    // universal ptr points to universal obj
    ptsGraph[nodeFactory.getUniversalPtrNode()].insert(nodeFactory.getUniversalObjNode());
    // universal obj points to universal obj
    ptsGraph[nodeFactory.getUniversalObjNode()].insert(nodeFactory.getUniversalObjNode());
    // null ptr points to null obj
    ptsGraph[nodeFactory.getNullPtrNode()].insert(nodeFactory.getNullObjectNode());
    // null obj points to nothing, so empty

    for (auto const& gv: M->globals()) {
        const GlobalValue *globalVal = &gv;
        if (globalVal->isDeclaration()) {
            auto itr = Ctx->Gobjs.find(globalVal->getName().str());
            if (itr != Ctx->Gobjs.end())
                globalVal = itr->second;
        }

        NodeIndex valNode = nodeFactory.createValueNode(globalVal);
        type = globalVal->getType();
        if (type->isPointerTy())
            createNodeForPointerVal(globalVal, type, valNode, ptsGraph);

        // FIXME: static initializer should be processed, but maybe not here
    }

    for (auto const& f: *M) {
        if (f.hasAddressTaken()) {
            NodeIndex fVal = nodeFactory.createValueNode(&f);
            NodeIndex fObj = nodeFactory.createObjectNode(&f);
            ptsGraph[fVal].insert(fObj);
        }
    }

    // Create return node
    type = F->getReturnType();
    if (!type->isVoidTy()) {
        valNode = nodeFactory.createReturnNode(F);
        // TODO: create return object later
    }

    // Create vararg node
    if (F->getFunctionType()->isVarArg())
        nodeFactory.createVarargNode(F);

    // Add nodes for all formal arguments.
    for (auto const &a : F->args()) {
        const Argument *arg = &a;
        valNode = nodeFactory.createValueNode(arg);

        type = arg->getType();
        if (type->isPointerTy())
            createNodeForPointerVal(arg, type, valNode, ptsGraph);
    }
}

NodeIndex PointToAnalysis::processStruct(const Value* v, const StructType* stType, const NodeIndex ptr, PtsGraph &ptsGraph) {
    // FIXME: Hope opaque type does not happen in kcfi. We linked whole kernel
    // chunks into the single IR file, and thus there shouldn't be any forward
    // declaration. If there's still, I don't think we can handle this case
    // anyway? :( For now, simply turned off the assertion to test.

#if 0
    // We cannot handle opaque type
    if (stType->isOpaque()) {
        errs() << "Opaque struct type ";
        stType->print(errs());
        errs() << "\n";
        return;
    }
    // assert(!stType->isOpaque() && "Opaque type not supported");
#endif

    // Sanity check
    assert(stType != NULL && "structType is NULL");
    //OP<<"2. stType:"<<*stType<<"\n";
    //OP<<"3. module name:"<<module->getName().str()<<"\n";

    const StructInfo* stInfo = Ctx->structAnalyzer.getStructInfo(stType, M);
    assert(stInfo != NULL && "structInfoMap should have info for all structs!");

    // Empty struct has only one pointer that points to nothing
    if (stInfo->isEmpty()) {
        ptsGraph[ptr].insert(nodeFactory.getNullObjectNode());
        return nodeFactory.getNullObjectNode();
    }

    // Non-empty structs: create one pointer and one target for each field
    uint64_t stSize = stInfo->getExpandedSize();

    // We construct a target variable for each field
    // A better approach is to collect all constant GEP instructions and construct variables only if they are used. We want to do the simplest thing first
    NodeIndex obj = nodeFactory.getObjectNodeFor(v);
    if (obj == AndersNodeFactory::InvalidIndex) {
        obj = nodeFactory.createObjectNode(v);
        if (stInfo->isFieldPointer(0))
            ptsGraph[obj].insert(nodeFactory.getUniversalObjNode());
        //OP << "expanded size = " << stSize << "\n";
        for (unsigned i = 1; i < stSize; ++i) {
            NodeIndex structObj = nodeFactory.createObjectNode(obj, i);
            // pointer field points to univeral
            if (stInfo->isFieldPointer(i))
                ptsGraph[structObj].insert(nodeFactory.getUniversalObjNode());
        }
    }

    ptsGraph[ptr].insert(obj);
    return obj;
}

PtsGraph PointToAnalysis::processInstruction(const Instruction *I, PtsGraph &in) {

    // handle constant GEPExpr oprands here
    for (auto const &opr : I->operands()) {
        const Value *v = opr.get();
        if (const ConstantExpr* ce = dyn_cast<ConstantExpr>(v)) {
            if (ce->getOpcode() == Instruction::GetElementPtr) {
                NodeIndex gepIndex = nodeFactory.getValueNodeFor(v);
                in[gepIndex].insert(nodeFactory.getObjNodeForGEPExpr(gepIndex));
            }
        }
    }

    PtsGraph out = in;
    switch (I->getOpcode()) {
        case Instruction::Alloca: {
            NodeIndex valNode = nodeFactory.getValueNodeFor(I);
            assert (valNode != AndersNodeFactory::InvalidIndex && "Failed to find alloca value node");

            assert (I->getType()->isPointerTy());
            createNodeForPointerVal(I, I->getType(), valNode, out);
            break;
        }
        case Instruction::Load: {
            // val = load from op, due to ptr2int, consider all loads
            NodeIndex ptrNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(ptrNode != AndersNodeFactory::InvalidIndex && "Failed to find load operand node");
            NodeIndex valNode = nodeFactory.getValueNodeFor(I);
            assert(valNode != AndersNodeFactory::InvalidIndex && "Failed to find load value node");
            //OP << "ptr " << opIndex << ":\n";
            for (auto i : in[ptrNode]) {
#ifdef DBG_LOAD
                OP << i << ":\n";
                for (auto item:in[i]) {
                    OP << "item " << item << ", ";
                }
                OP << "\n";
#endif
                out[valNode].insert(in[i].begin(), in[i].end());
            }
            break;
        }
        case Instruction::Store: {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I->getOperand(1));
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find store dst node");
            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            if (srcNode == AndersNodeFactory::InvalidIndex) {
                // If we hit the store insturction like below, it is
                // possible that the global variable (init_user_ns) is
                // an extern variable. Hope this does not happen,
                // otherwise we need to assume the worst case :-(

                // 1. store %struct.user_namespace* @init_user_ns,
                // %struct.user_namespace**  %from.addr.i, align 8
                // 2. @init_user_ns = external global %struct.user_namespace
                errs() << "Failed to find store src node for " << *I << "\n";
                break;
            }
            // assert(srcIndex != AndersNodeFactory::InvalidIndex && "Failed to find store src node");

            if (in.find(srcNode) != in.end()) {
                for (auto i : in[dstNode])
                    out[i] = in[srcNode];
            }
            break;
        }
        case Instruction::GetElementPtr: {
            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find gep src node");
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find gep dst node");

            //OP << "Inside gep inst, before call the function getGepInstFieldNum:\n";
            //Ctx->structAnalyzer.printStructInfo();

            int64_t offset = getGEPOffset(I, DL);
            int fieldNum = 0;
            if (offset < 0) {
                // update the out pts graph instead of the in
                fieldNum = handleContainerOf(I, offset, srcNode, out);
                // XXX: negative encoded as unsigned
            } else {
                fieldNum = offsetToFieldNum(I->getOperand(0)->stripPointerCasts(), offset, DL,
                    &Ctx->structAnalyzer, M);
            }
            //OP << "filedNum = " << fieldNum << "\n";

            // since we may have updated the pts graph, we need to use the out instead of in
            for (auto n : out[srcNode]) {
                assert(nodeFactory.isObjectNode(n) && "src ptr does not point to obj node");
                if (n > nodeFactory.getConstantIntNode())
                    out[dstNode].insert(n + fieldNum);
                else
                    out[dstNode].insert(n);
            }
#ifdef gep_check
            OP << "srcIndex = "<<srcIndex << "\n";
            OP << "out[" << dstIndex << "]:\n";
            for (auto pts : out[dstIndex])
                OP << pts << ", ";
            OP<< "\n";
#endif
            break;
        }
        case Instruction::PHI: {
            const PHINode* PHI = cast<PHINode>(I);
            NodeIndex dstNode = nodeFactory.getValueNodeFor(PHI);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find phi dst node");
            for (unsigned i = 0, e = PHI->getNumIncomingValues(); i != e; ++i) {
                NodeIndex srcNode = nodeFactory.getValueNodeFor(PHI->getIncomingValue(i));
                assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find phi src node");
                if (in.find(srcNode) != in.end())
                    out[dstNode].insert(in[srcNode].begin(), in[srcNode].end());
            }
            break;
        }
        case Instruction::BitCast: {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find bitcast dst node");
            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find bitcast src node");

            assert(I->getType()->isPointerTy());
            Type *T = cast<PointerType>(I->getType())->getElementType();
            if (StructType *ST = dyn_cast<StructType>(T)) {
                const StructInfo* stInfo = Ctx->structAnalyzer.getStructInfo(ST, M);
                assert(stInfo != NULL && "Failed to find stinfo");

                unsigned stSize = stInfo->getExpandedSize();
                assert(!in[srcNode].isEmpty());
                for (NodeIndex obj : in[srcNode]) {
                    unsigned objSize = nodeFactory.getObjectSize(obj);
                    if (stSize > objSize) {
#if 0
                        OP << "BitCast: " << *I << ":"
                           << " dst size " << stSize
                           << " larger than src size " << objSize << "\n";
#endif
                        NodeIndex newObj = extendObjectSize(obj, ST, out);
                    }
                }
            }
            // since we may have updated the pts graph, we need to use the out instead of in
            out[dstNode] = out[srcNode];
            break;
        }
        case Instruction::Trunc: {
            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find trunc src node");
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find trunc dst node");
            if (in.find(srcNode) != in.end())
                out[dstNode] = in[srcNode];
            break;
        }
        case Instruction::IntToPtr: {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr dst node");
            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr src node");
            // ptr to int to ptr
            if (in.find(srcNode) != in.end())
                out[dstNode] = in[srcNode];
            break;
        }
        case Instruction::PtrToInt: {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr dst node");
            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr src node");
            out[dstNode] = in[srcNode];
            break;
        }
        case Instruction::Select: {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find select dst node");

            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(1));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find select src node 1");
            if (in.find(srcNode) != in.end())
                out[dstNode].insert(in[srcNode].begin(), in[srcNode].end());

            srcNode = nodeFactory.getValueNodeFor(I->getOperand(2));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find select src node 2");
            if (in.find(srcNode) != in.end())
                out[dstNode].insert(in[srcNode].begin(), in[srcNode].end());

            break;
        }
        case Instruction::VAArg: {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find va_arg dst node");
            NodeIndex vaNode = nodeFactory.getVarargNodeFor(I->getParent()->getParent());
            assert(vaNode != AndersNodeFactory::InvalidIndex && "Failed to find va_arg node");
            if (in.find(vaNode) != in.end())
                out[dstNode] = in[vaNode];
            break;
        }
        case Instruction::Call: {
            if (I->getType()->isPointerTy()) {
                NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
                assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find call dst node");
                out[dstNode].clear();
                createNodeForPointerVal(I, I->getType(), dstNode, out);
            }
            break;
        }
        case Instruction::Ret: {
            if (I->getNumOperands() != 0) {
                Value *RV = I->getOperand(0);
                NodeIndex valNode = nodeFactory.getValueNodeFor(RV);
                assert(valNode != AndersNodeFactory::InvalidIndex && "Failed to find call return value node");
                if (RV->getType()->isPointerTy()) {
                    NodeIndex retNode = nodeFactory.getReturnNodeFor(F);
                    assert(retNode != AndersNodeFactory::InvalidIndex && "Failed to find call return node");
                    out[retNode] = in[valNode];
                } else {
                    if (!isa<ConstantInt>(RV)) {
                        auto itr = in.find(valNode);
                        assert(itr == in.end() || itr->second.isEmpty() ||
                            (itr->second.getSize() == 1 &&
                             itr->second.has(nodeFactory.getConstantIntNode())
                            )
                        );
                    }
                }
            }
            break;
        }
        default: {
            if (I->getType()->isPointerTy())
                OP << "pointer-related inst not handled!" << *I << "\n";
                //WARNING("pointer-related inst not handled!" << *I << "\n");
            //assert(!inst->getType()->isPointerTy() && "pointer-related inst not handled!");
            break;
        }
    } //switch(I->getOpCode())
    return out;
}

void PointToAnalysis::unionPtsGraph(PtsGraph &pg, const PtsGraph &other) {
    for (auto itr2 = other.begin(), end2 = other.end(); itr2 != end2; ++itr2) {
        auto itr1 = pg.find(itr2->first);
        if (itr1 != pg.end())
            itr1->second.insert(itr2->second.begin(), itr2->second.end());
        else
            pg.insert(std::make_pair(itr2->first, itr2->second));
    }
}

// given old object node and new struct info, extend the object size
// return the new object node
NodeIndex PointToAnalysis::extendObjectSize(NodeIndex oldObj, const StructType* stType, PtsGraph &ptsGraph) {
    // FIXME: assuming oldObj is the base
    assert(nodeFactory.getObjectOffset(oldObj) == 0);

    const Value *val = nodeFactory.getValueForNode(oldObj);
    assert(val != nullptr && "Failed to find value of node");
    NodeIndex valNode = nodeFactory.getValueNodeFor(val);

    // before creating new obj, remove the old ptr->obj
    auto itr = ptsGraph.find(valNode);
    assert(itr != ptsGraph.end() && itr->second.has(oldObj));
    itr->second.reset(oldObj);
    nodeFactory.removeNodeForObject(val);

    // create new obj
    NodeIndex newObj = processStruct(val, stType, valNode, ptsGraph);

    // update ptr2set
    updateObjectNode(oldObj, newObj, ptsGraph);

    return newObj;
}

// given the old object node, old size, and the new object node
// replace all point-to info to the new node
// including the value to object node mapping
void PointToAnalysis::updateObjectNode(NodeIndex oldObj, NodeIndex newObj, PtsGraph &ptsGraph) {
    unsigned offset = nodeFactory.getObjectOffset(oldObj);
    NodeIndex baseObj = nodeFactory.getOffsetObjectNode(oldObj, -(int)offset);
    unsigned size = nodeFactory.getObjectSize(oldObj);

    // well, really expensive op
    // use explicit iterator for updating
    for (auto itr = ptsGraph.begin(), end = ptsGraph.end(); itr != end; ++itr) {
        AndersPtsSet temp = itr->second;
        // in case modification will break iteration
        for (NodeIndex obj : temp) {
            if (obj >= baseObj && obj < (baseObj + size)) {
                itr->second.reset(obj);
                itr->second.insert(newObj + (obj - baseObj));
            }
        }
    }
}

int PointToAnalysis::handleContainerOf(const Instruction* I, int64_t offset, NodeIndex srcNode, PtsGraph &ptsGraph) {
    assert(I->getType()->isPointerTy() && "passing non-pointer type to handleContainerOf");
    assert(cast<PointerType>(I->getType())->getElementType()->isIntegerTy(8) && "handleContainerOf can only handle i8*");
    assert(offset < 0 && "handleContainerOf can only handle negative offset");

    // for each obj the pointer can point-to
    // hopefully there'll only be one obj
    assert(ptsGraph[srcNode].getSize() == 1);
    for (NodeIndex obj : ptsGraph[srcNode]) {
        // if src obj special, ptr arithmetic is meaningless
        if (obj <= nodeFactory.getConstantIntNode())
            return 0;

        // find the allocated type
        const Value *allocObj = nodeFactory.getValueForNode(obj);
        const Type *allocType = allocObj->getType();

        // 1. check if allocated type is okay
        assert(allocType->isPointerTy());
        const StructType *stType = dyn_cast<StructType>(cast<PointerType>(allocType)->getElementType());
        assert(stType && "handleContainerOf can only handle struct type");

        const StructInfo* stInfo = Ctx->structAnalyzer.getStructInfo(stType, M);
        assert(stInfo != NULL && "structInfoMap should have info for st!");

        // find the size offset of src ptr
        // int64_t should be large enough for unsigned
        int64_t srcField = nodeFactory.getObjectOffset(obj);
        int64_t sizeOffset = stInfo->getFieldOffset(srcField);
        // check if the size offset of src ptr is large enough to handle the negative offset
        if (sizeOffset + offset >= 0) {
            // FIXME: assume only one obj
            int64_t dstField = offsetToFieldNum(I->getOperand(0)->stripPointerCasts(), sizeOffset + offset,
                DL, &Ctx->structAnalyzer, M);
            return (int)(dstField - srcField);
        }

        // 2. If the allocated type is not large enough to handle the offset,
        //    create a new one and update the ptr2set
        
        // get real type
        const Type *realType = nullptr;
        const Value *castInst = nullptr;
        for (auto U : I->users()) {
            if (isa<BitCastInst>(U)) {
                realType = U->getType();
                castInst = U;
                break;
            }
        }
        assert(realType && "Failed to find the dst type for container_of");
        assert(realType->isPointerTy());
        const StructType *rstType = dyn_cast<StructType>(cast<PointerType>(realType)->getElementType());
        assert(rstType && "handleContainerOf can only handle struct type");

        // this is the tricky part, we need to find the field number inside the larger struct
        const StructInfo *rstInfo = Ctx->structAnalyzer.getStructInfo(rstType, M);
        assert(rstInfo != nullptr && "structInfoMap should have info for rst!");

        // FIXME: assuming one container_of at a time
        // fix stInfo if srcField is not 0
        bool found_container = false;
        std::set<Type*> elementTypes = stInfo->getElementType(srcField);
        for (Type *t : elementTypes) {
            if (StructType *est = dyn_cast<StructType>(t)) {
                const StructInfo *estInfo = Ctx->structAnalyzer.getStructInfo(est, M);
                assert(estInfo != nullptr && "structInfoMap should have info for est!");
                if (estInfo->getContainer(rstInfo->getRealType(), -offset)) {
                    found_container = true;
                    break;
                }
            }
        }
        assert(found_container && "Failed to find the container");

#if 0
        int64_t currentOffset = offset;
        const Type *subType = nullptr;
        for (StructType::element_iterator itr = st->element_begin(), ite = st->element_end(); itr != ite; ++itr) {
            subType = *itr;
             // stop after getting the next type
            if (currentOffset >= 0)
                break;

            bool isArray = false;
            if (const ArrayType *arrayType = dyn_cast<ArrayType>(subType)) {
                currentOffset += (layout->getTypeAllocSize(arrayType->getElementType()) * arrayType->getNumElements());
                isArray = true;
            }

            while (const ArrayType *arrayType = dyn_cast<ArrayType>(subType))
                subType = arrayType->getElementType();

            if (const StructType *structType = dyn_cast<StructType>(subType)) {
                subInfo = Ctx->structAnalyzer.getStructInfo(structType, M);
                currentOffset += subInfo->getAllocSize();
            } else if (!isArray) {
                currentOffset += rstInfo->getDataLayout()->getTypeAllocSize(subType);
            }
        }
        assert(currentOffset == 0 && "Offset desn't points to the beginning of the container");
        assert(isCompatibleType(stType, subType) && "Sub-type doesn't match");
#endif

        // okay, now we have find the correct container
        // create the new obj, using the castinst as base
        // This is an ugly hack, because we don't really have an original obj
        NodeIndex valNode = nodeFactory.getValueNodeFor(castInst);
        assert(valNode != AndersNodeFactory::InvalidIndex && "Failed to find castinst node");
        NodeIndex castObjNode = processStruct(castInst, rstType, valNode, ptsGraph);

        // next, update the ptr2set to the new object
        unsigned subField = offsetToFieldNum(castInst, -offset, DL, &Ctx->structAnalyzer, M);
        NodeIndex newObjNode = nodeFactory.getOffsetObjectNode(castObjNode, subField);

        updateObjectNode(obj, newObjNode, ptsGraph);

        // update the value -> obj map
        nodeFactory.updateNodeForObject(allocObj, newObjNode);

        // finally, return the offset/fieldNum
        return -subField;
    } // end for all obj in ptr2set
    return 0;
}
