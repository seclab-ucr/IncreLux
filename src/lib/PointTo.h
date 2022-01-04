#ifndef UBIANALYSIS_POINTTO_H
#define UBIANALYSIS_POINTTO_H

#include <map>

#include "UBIAnalysis.h"
#include "NodeFactory.h"
#include "PtsSet.h"

typedef std::map<NodeIndex, AndersPtsSet> PtsGraph;
typedef std::map<const llvm::Instruction*, PtsGraph> NodeToPtsGraph;

class PointToAnalysis {
private:
    llvm::Function *F;
    const llvm::DataLayout* DL;
    llvm::Module *M;
    GlobalContext *Ctx;

    AndersNodeFactory nodeFactory;
    NodeToPtsGraph nPtsGraph;

    //utils
    void createNodeForPointerVal(const llvm::Value*, const llvm::Type*, const NodeIndex, PtsGraph&);
    void createInitNodes(PtsGraph&);
    PtsGraph processInstruction(const llvm::Instruction*, PtsGraph&);
    NodeIndex processStruct(const llvm::Value*, const llvm::StructType*, const NodeIndex, PtsGraph&);
    void unionPtsGraph(PtsGraph&, const PtsGraph&);
    NodeIndex extendObjectSize(NodeIndex, const llvm::StructType*, PtsGraph&);
    void updateObjectNode(NodeIndex, NodeIndex, PtsGraph&);
    int handleContainerOf(const llvm::Instruction*, int64_t, NodeIndex, PtsGraph&);
public:
    PointToAnalysis(llvm::Function *F_, GlobalContext *Ctx_)
            : F(F_), Ctx(Ctx_) {
        M = F->getParent();
        DL = &(M->getDataLayout());
        nodeFactory.setDataLayout(DL);
        nodeFactory.setStructAnalyzer(&(Ctx->structAnalyzer));
        nodeFactory.setGobjMap(&(Ctx->Gobjs));
    }
    bool run();
};

int64_t getGEPInstFieldNum(const llvm::GetElementPtrInst* gepInst,
    const llvm::DataLayout* dataLayout, const StructAnalyzer& structAnalyzer, llvm::Module* module);

#endif
