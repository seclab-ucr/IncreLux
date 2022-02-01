//
// Created by ubuntu on 2/8/18.
//

//#include "llvm/IR/Module.h"
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

#define SERIALIZATION
//#define OOB_IO
//#define LOADSUM
//#define TIMING
#define RUN_ON_FUNC
//#define WRITEJSON
//#define TIMING
//#define TEST
//#define READYLIST_DEBUG
//#define _INIT_PTS
//#define _PRINT_NODEFACTORY
//#define _PRINT_INST
////#define DEBUG_TITLE
//#define _PRINT_SUMMARY
//#define _PRINT_SUMNODEFACT
//#define _PRINT_PTS
//#define IN
//#define OUT
//#define gep_check
// #define _INIT_NF
//#define _DBG
#define _FIND_DEP
//#define _PRINT_SCC
//#define _PRINT_DEPCG
//#define PRINT_BIGCIRCLE
//#define CAL_BIGCIRCLE
//#define OLD_COLLECT
//#define CAL_STACKVAR
using namespace llvm;
static bool isCastingCompatibleType(Type *T1, Type *T2);
std::set<std::string> timer_set{"common_timer_del", "common_timer_create", "common_hrtimer_rearm",
            "common_hrtimer_try_to_cancel", "common_hrtimer_arm","common_timer_get"};
std::string DBGCaller = "common_timer_del";//"common_timer_set";
int dbgcount = 0;
const int threshold = 0;
std::string keyfunc = "common_timer_del";
bool QualifierAnalysis::doInitialization(llvm::Module *M)
{
    return false;
}

bool QualifierAnalysis::doModulePass(llvm::Module *M)
{
    return false;
}
std::unordered_map<llvm::Function*, std::set<llvm::Function*>> mem;

void printCallees(llvm::Function* func) {
	for (inst_iterator itr = inst_begin(*func), ite = inst_end(*func); itr != ite; ++itr){
		auto inst = itr.getInstructionIterator();
        	if (isa<CallInst>(&*inst))
        	{   
            	    CallInst *CI = dyn_cast<CallInst>(&*inst);
		    if (CI->getCalledFunction()) {
			llvm::Function *Callee = CI->getCalledFunction();
			if (Callee->getName().str() == "cdrom_ioctl") {
			    errs()<<*CI<<"\n";
			    errs()<<Callee<<" : "<<Callee->getName().str();
			}
		    }
        	}
	}
}
std::set<llvm::Function*> calCallee(llvm::Function *func, CallMap &CM) {
    if (mem.count(func)) return mem[func];
    std::set<llvm::Function*> ans;
    //OP<<"inside calcallee for"<<func->getName().str()<<"\n";
    if (CM.find(func) != CM.end()) {
        for (auto item : CM[func]) {
	    ans.insert(item);
	    ans.insert(mem[item].begin(), mem[item].end());
        }
    }
    mem[func] = ans;
    return ans;
}
void Tarjan::dfs(llvm::Function *F)
{
    dfn[F] = low[F] = ts++;
    depVisit.insert(F);
    Stack.push(F);
    inStack[F] = true;
    for (auto iter : depCG[F])
    {
        llvm::Function *func = iter;
        if (!depVisit.count(func))
        {
            dfs(func);
            if (low[func] < low[F])
            {
                low[F] = low[func];
            }
        }
        else
        {
            if (dfn[func] < low[F] && inStack[func])
            {
                low[F] = dfn[func];
            }
        }
    }

    llvm::Function *temp = NULL;
    sc.clear();
    if (dfn[F] == low[F])
    {
        do
        {
            temp = Stack.top();
            sc.push_back(temp);
            Stack.pop();
            inStack[temp] = false;
        } while (temp != F);

        SCC.push_back(sc);
	int n = sc.size();
	if (n > biggest) biggest = n;
        if (sc.size()==1 && depCG[sc.back()].count(sc.back())) {
            rec++;
        }
        sccSize+=sc.size();
    }
}
void Tarjan::getSCC(std::vector<std::vector<llvm::Function *>> &ans)
{
    int count = 0;
    for (auto iter : funcs)
    {
        llvm::Function *cur = iter;
        if (depVisit.count(cur))
            continue;
        dfs(cur);
        count++;
        //OP<<"count = "<<count<<", function :"<<cur->getName().str()<<"\n";
    }
    ans = SCC;
    OP<<"partition size : "<<count<<", sccSize = " << sccSize << ", rec size = "<<rec<<", biggest = "<<biggest<<"\n";
}


void QualifierAnalysis::collectRemaining()
{
}
void QualifierAnalysis::calDepFuncs()
{
}
bool QualifierAnalysis::doFinalization(llvm::Module *M)
{
    module = M;
    DL = &(M->getDataLayout());
    bool ret = false;
    return ret;
}
void QualifierAnalysis::getGlobals() {
    for (auto item : Ctx->Modules) {
        llvm::Module *M = item.first;
        for (GlobalVariable &G : M->globals())
        {
            if (G.hasExternalLinkage())
                Ctx->Gobjs[G.getName().str()] = &G;
        }
    #ifdef LOADSUM
    for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f)
    {
	Function *F = &*f;
	//getDef(F);
	std::string fname = getScopeName(F, Ctx->funcToMd, Ctx->Funcs);
	std::string sFile = Ctx->outFolder+"/Summary/"+fname+".sum";
	if (Ctx->incAnalysis) {
	    sFile = Ctx->incFolder+"/Summary/" + fname + ".sum";
	}
	std::ifstream ifile(sFile);
	OP<<"sFile:"<<sFile<<"\n";
 	if (ifile)
	{
		OP<<"file exists!\n";
		//std::ifstream ifile(sFile);
		boost::archive::text_iarchive ia(ifile);
		ia>>Ctx->FSummaries[F];
		Ctx->Visit[F] = true;
		Ctx->ReadyList.insert(F);	
		//Ctx->FSummaries[F].summary();
		ifile.close();
		FCounter++;
		OP<<"load function "<<fname<<"\n";
		//Ctx->FSummaries[F].summary();
		OP<<FCounter<<" Function Summaries Loaded!\n";
		for (Function *caller : Ctx->CalledMaps[F]) {
        		Ctx->RemainedFunction[caller]--;
            		if (Ctx->RemainedFunction[caller] == 0) {
            			Ctx->ReadyList.insert(caller);
            		}
        	}
	}
    }
    #endif
    }
}
void QualifierAnalysis::run() {
    getGlobals();
    /*Deal with functions that do not exists in CallMaps, now they are in the indFuncs*/
    int counter = 0;
    Tarjan tarjan(Ctx->CallMaps);
    tarjan.getSCC(Ctx->SCC);
    OP<<"size of indiFuncs : "<<Ctx->indFuncs.size()<<"\n";
    #ifdef _PRINT_SCC
	int scCount=0;
	for (auto sc : Ctx->SCC) {
	    OP<<"=="<<scCount++<<" : sc===\n";
	    for (auto func : sc) {
		OP<<func->getName().str()<<"/";
	    }
	    OP<<"\n";
	}
    return;
    #endif
    for (auto f : Ctx->indFuncs) {
        #ifdef RUN_ON_FUNC
        runOnFunction(f, true);
        #endif
        Ctx->Visit[f] = true;
        counter++;
        OP<<counter<<" Functions Finished.\n";
    }
    unsigned temp = 0;
    for (auto sc : Ctx->SCC) {
        llvm::Function *func = sc.back();
	if (sc.size() == 1 && (!Ctx->CallMaps[func].count(func))) {
	    Ctx->impact[func] = calCallee(func, Ctx->CallMaps);
            #ifdef RUN_ON_FUNC
            runOnFunction(func, true);
            #endif
            Ctx->Visit[func] = true;
        }
        else {
            calSumForRec(sc);
        }
        counter += sc.size();
        OP<<counter<<" Functions Finished.\n";
    }
    return;
}
void QualifierAnalysis::runInc() {
    getGlobals();
    /*Deal with functions that do not exists in CallMaps, now they are in the indFuncs*/
    int counter = 0;
    OP<<"size of indiFuncs : "<<Ctx->indFuncs.size()<<"\n";
    for (auto f : Ctx->indFuncs) {
        #ifdef RUN_ON_FUNC
            runOnFunction(f, true);
        #endif
            Ctx->Visit[f] = true;
        counter++;
        OP<<counter<<" Functions Finished.\n";
    }

    /*calculate SCC for candiCG*/
    Tarjan tarjan(Ctx->candiCallMaps);
    tarjan.getSCC(Ctx->SCC);

    /*Doing bottom-up analysis and see if the current function need to be analyzed*/
    for (auto sc : Ctx->SCC) {
        llvm::Function *func = sc.back();
	bool analysis = false;
	for (auto item : sc){
	    llvm::Function *cur = &*item;
	    if (Ctx->modifiedFuncs.count(cur)) {
		analysis = true;
		break;
	    }
	}
	if (!analysis) continue;
#ifdef OP_DBG
        OP<<"sc.size() = "<<sc.size()<<"\n";
	OP<<"modifiedFuncs.size() = "<<Ctx->modifiedFuncs.size()<<"\n";
        if (sc.size() == 1) {
            if (Ctx->candiCallMaps[func].count(func)) {
                OP<<"Rec: "<<func->getName().str()<<"\n";                    
            }
            else {
                OP<<"Non-rec : "<<func->getName().str()<<"\n";
            }
        }
#endif
	int total_analyzed = 1;
        /*Non-recursive functions*/
	if (sc.size() == 1 && (!Ctx->candiCallMaps[func].count(func))) {
            /*The function is changed and need to be analyzed*/
	    //OP<<"Run on non-rec function:\n";
            if (Ctx->modifiedFuncs.find(func) != Ctx->modifiedFuncs.end()) {
                if (Ctx->FSummaries.find(func) == Ctx->FSummaries.end()) {
	    		std::string fname = getScopeName(func, Ctx->funcToMd, Ctx->Funcs);
	    		//std::string cur = "lll-56-1";
			//std::string cur = "lll-415";
    			size_t pos = fname.find(Ctx->newVersion);
	    		if (pos  != std::string::npos) {
				fname.replace(pos, Ctx->newVersion.size(), Ctx->oldVersion);
	    		}
	    
	    		//OP<<"fname = "<<fname<<"\n";
            		std::string sFile = Ctx->outFolder+"/Summary/"+fname+".sum";
	   		#ifdef OOB_IO
			std::string iosFile = Ctx->outFolder+"/IOSummary/"+fname+".iosum";
			std::string oobsFile = Ctx->outFolder+"/OOBSummary/"+fname+".oobsum";
			#endif
			OP<<"sFile = "<<sFile<<"\n";
           		std::ifstream ifile(sFile);   
	    		if (ifile){
               		    OP<<"2file exists!\n";
               		    //std::ifstream ifile(sFile);
               		    boost::archive::text_iarchive ia(ifile);
               		    ia>>Ctx->FSummaries[func];
           		}
                        else {
                       	    //OP<<"2file not found!\n";
               		}
			#ifdef OOB_IO
			std::ifstream iofile(iosFile);
			OP<<"iosFile = "<<iosFile<<"\n";
			if (iofile) {
				boost::archive::text_iarchive ia(iofile);
                                ia>>Ctx->IOFSummaries[func];
			}
			else{
			    //OP<<"2iofile not found!\n";
			}
			std::ifstream oobfile(oobsFile);
			OP<<"oobsFile = "<<oobsFile<<"\n";
                        if (oobfile){
                             //OP<<"oobfile exists!\n";
                             //std::ifstream ifile(sFile); 
                             boost::archive::text_iarchive ia(oobfile);
                             ia>>Ctx->OOBFSummaries[func];
                        }
                        else {
                             //OP<<"2oobfile not found!\n";
                        }
			#endif
		}
                Summary oldSummary = Ctx->FSummaries[func];
		#ifdef OOB_IO
		Summary oldIOSummary = Ctx->IOFSummaries[func];
		Summary oldOOBSummary = Ctx->OOBFSummaries[func];
		#endif
		OP<<"---oldSummary for "<<func->getName().str()<<"\n";
		oldSummary.summary();
                #ifdef RUN_ON_FUNC
                runOnFunction(func, true);
                #endif
                Summary newSummary = Ctx->FSummaries[func];
		OP<<"---newSummary for "<<func->getName().str()<<"\n";
                newSummary.summary();
#ifdef OOB_IO
		if (!newSummary.equal(oldSummary) || !Ctx->IOFSummaries[func].equal(oldIOSummary)
			|| !Ctx->OOBFSummaries[func].equal(oldOOBSummary)) {
		    if (!newSummary.equal(oldSummary)) {OP<<"UBI Summary not equal.\n";}
		    if (!Ctx->IOFSummaries[func].equal(oldIOSummary)) {OP<<"IO Summary not equal. \n";}
		    if (!Ctx->OOBFSummaries[func].equal(oldOOBSummary)) {OP<<"OOB Summary not equal. \n";}

#else
		if (!newSummary.equal(oldSummary)) {
#endif
		    OP<<"UBI Summary not equal.\n";
	    	    /*put the caller into list for re-calculation*/
	    	    for (auto item : Ctx->candiCalledMaps[func]) {
			Ctx->incCandiFuncs[item] = true;
			//OP<<"insert function "<<item->getName().str()<<" to modifiedFucs\n";
			Ctx->modifiedFuncs.insert(item);
	    	    }
		}
            }
            Ctx->Visit[func] = true;
        }
        else {            
            //OP<<"incCalSumForRec : \n";
            //for (auto item : sc) {
             //   OP<<item->getName().str()<<"/";
            //}
            //OP<<"\n";

            total_analyzed = incCalSumForRec(sc);
            for (auto item : sc) {
                Ctx->Visit[item] = true;
            }
        }
        counter += total_analyzed;
        OP<<counter<<" Functions Finished.\n";
    }
}
int QualifierAnalysis::incCalSumForRec(std::vector<llvm::Function*>& scc) {
    //OP<<"Inside incCalSumForRec: \n";
    /*Check if any function inside the scc are modified*/
    std::queue<llvm::Function*> worklist;
    std::unordered_set<llvm::Function*> sccSet;
    std::unordered_map<llvm::Function*, bool> inQueue;
    std::unordered_map<llvm::Function*, int> aTimes;
    int ret = 0;
    for (auto item : scc) {
	sccSet.insert(item);
        if(Ctx->incCandiFuncs[item]) {
            worklist.push(item);
	    inQueue[item] = true;
	    Ctx->Visit[item] = false;
        }
    }
    /*No function changes, thus just return.*/
    if (worklist.empty()) return ret;
    
    /*Some functions change, we need to calculate the scc till no summary changes*/
    std::unordered_map<llvm::Function*, Summary> oldSum;
#ifdef OOB_IO
    std::unordered_map<llvm::Function*, Summary> oldIOSum;
    std::unordered_map<llvm::Function*, Summary> oldOOBSum;
#endif

    while (!worklist.empty()) {
	llvm::Function *item = worklist.front();
        worklist.pop();
	inQueue[item] = false;
	if (aTimes[item] == 0) ret += 1;
	aTimes[item]++;
	if (Ctx->incCandiFuncs[item]) {
	    Ctx->modifiedFuncs.insert(item);
	    if (Ctx->FSummaries.find(item) == Ctx->FSummaries.end()){
		std::string fname = getScopeName(item, Ctx->funcToMd, Ctx->Funcs);
		size_t pos = fname.find(Ctx->newVersion);
		if (pos  != std::string::npos) {
                    fname.replace(pos, Ctx->newVersion.size(), Ctx->oldVersion);
                }
		std::string sFile = Ctx->outFolder+"/Summary/"+fname+".sum";
		std::ifstream ifile(sFile);
		if (ifile){
                    boost::archive::text_iarchive ia(ifile);
                    ia>>Ctx->FSummaries[item];
                }
		#ifdef OOB_IO
		std::string iosFile = Ctx->outFolder+"/IOSummary/"+fname+".sum";
		std::ifstream iofile(iosFile);
		if (iofile){
                    boost::archive::text_iarchive ia(iofile);
                    ia>>Ctx->IOFSummaries[item];
                }
		
		std::string oobsFile = Ctx->outFolder+"/OOBSummary/"+fname+".sum";
                std::ifstream oobfile(oobsFile);
                if (oobfile){
                    boost::archive::text_iarchive ia(oobfile);
                    ia>>Ctx->OOBFSummaries[item];
                }
		#endif	
	    }//FSummaries not find item
	    Summary oldSummary = Ctx->FSummaries[item];
	    Summary oldIOSummary = Ctx->IOFSummaries[item];
	    Summary oldOOBSummary = Ctx->OOBFSummaries[item];
	    #ifdef RUN_ON_FUNC
            runOnFunction(item, false);
            #endif
	    Ctx->Visit[item] = true;
	    if (!Ctx->FSummaries[item].equal(oldSummary) || !Ctx->IOFSummaries[item].equal(oldIOSummary)
			|| !Ctx->OOBFSummaries[item].equal(oldOOBSummary)) {
		for (auto caller : Ctx->candiCalledMaps[item]) {
		    Ctx->incCandiFuncs[caller] = true;
		    if (!Ctx->FSummaries[item].equal(oldSummary)) {OP<<"2UBI Summary not equal.\n";}
                    if (!Ctx->IOFSummaries[item].equal(oldIOSummary)) {OP<<"2IO Summary not equal. \n";}
                    if (!Ctx->OOBFSummaries[item].equal(oldOOBSummary)) {OP<<"2OOB Summary not equal. \n";}

		    OP<<"insert function "<<caller->getName().str()<<" to modifiedFucs\n";
                    Ctx->modifiedFuncs.insert(caller);
		    if (sccSet.count(caller)&&aTimes[caller]<threshold) {
			Ctx->Visit[caller] = false;
			if (!inQueue[caller]) {
			    worklist.push(caller);
			    inQueue[caller] = true;
			}
		    }
		}
	    }
	}
    }//while (!worklist.empty())
    /*Output warnings and clear the fToWarns*/
    printWarnForScc(scc);
    return ret;
}
void QualifierAnalysis::printWarnForScc(std::vector<llvm::Function*>& scc) {
#ifdef WRITEJSON
    std::ofstream jfile;
    if (Ctx->incAnalysis){
        jfile.open(Ctx->jsonInc, std::ios::app);
    }
    else {
        jfile.open(Ctx->jsonFile, std::ios::app);
    }
#endif
    for (llvm::Function *F : scc) {
        if (!Ctx->incCandiFuncs[F])
            continue;
        
        auto warnSet = Ctx->fToWarns[F];
        //OP<<"Warnings of Function "<<F->getName().str()<<", size = "<<Ctx->fToWarns[F].size()<<"\n";
        for (auto item : warnSet) {
            //OP<<"bclist : "<<item.first<<"\n";
            //OP<<"jsonboj: "<<item.second<<"\n";
            #ifdef WRITEJSON
            //print the related bc files:
            jfile << item.first << "\n";
            jfile << item.second << "\n";
            #endif
        }
    }
    #ifdef WRITEJSON
    jfile.close();
    #endif
    Ctx->fToWarns.clear();
}
void QualifierAnalysis::finalize()
{
    OP << "Inside finalize(): \n";
#ifdef COLLECT_FINAL
    for (std::map<llvm::Function *, bool>::iterator i = Ctx->ChangeMap.begin(), e = Ctx->ChangeMap.end();
         i != e; i++)
    {
        //run all the remaining functions and do qualifier check
        module = i->first->getParent();
        DL = &(module->getDataLayout());
        runOnFunction(i->first, true);
        FCounter++;
        OP << FCounter << " Function Finished!\n";
    }
#endif
    int count = 0;
    if (Ctx->incAnalysis)
    {
        for (auto item : Ctx->incCandiFuncs)
        {
            if (item.second == false)
            {
                OP<<"do not need analysys: "<<item.first->getName().str()<<"\n";
                OP<<"remained function : "<<Ctx->incFuncRemained[item.first]<<": \n";
                for (auto cf : Ctx->candiCallMaps[item.first])
                {
                        //OP<<"cf: "<<cf->getName().str()<<"\n";
                        if (!Ctx->Visit[cf] && Ctx->incCandiFuncs[cf])
                        {
                            OP << "--" << count++ << ". " << cf << ": " << cf->getName().str();
                            if (cf->empty())
                                OP << " : Empty.";
                            if (Ctx->incCandiFuncs[cf]) {
                                OP << " : true.\n";
                            }
                            else {
                                OP << " : false.\n";
                            }
                            OP << "\n";
                        }
                    }
                count++;
                Ctx->Visit[item.first] = true;
            }
        }
    }
    OP<<count<<" funtions do not need to be analyzed.\n";
#ifdef RM
    if (Ctx->incAnalysis)
    {
        std::ofstream jfile;
        jfile.open(Ctx->jsonInc, std::ios::app);
        for (llvm::Function *F : Ctx->modifiedFuncs)
        {
            for (auto item : Ctx->modifiedFuncs)
            {
                if (Ctx->fToWarns.count(F))
                {
                    for (auto warn : Ctx->fToWarns[F])
                    {
                        jfile << warn.first << "\n";
                        jfile << warn.second << "\n";
                    }
                }
            }
        }
        jfile.close();
    }
#endif
}

bool QualifierAnalysis::runOnFunction(llvm::Function *f, bool flag)
{
    /*if (!Ctx->numSum.count(f)) {
	for (auto callee: Ctx->CallMaps[f]) {
	    Ctx->numSum[f]++;
	    Ctx->sumSize[f] += Ctx->FSummaries[callee].getSumSize();
	}
    }*/
    Ctx->times[f]++;
    if (Ctx->Visit[f]) return false;
    if ((f->hasName())&&(f->getName().str().find("llvm_gcov") != std::string::npos)) {
	    OP<<"skip gcov function : "<<f->getName().str()<<"\n";
 	    return false;
    }
#ifdef TIMING
    clock_t sTime, eTime;
    sTime = clock();
#endif
    llvm::Module *M = f->getParent();
    module = M;
    DL = &(M->getDataLayout());

    FuncAnalysis FA(f, Ctx,flag);
    FA.run();
#ifdef TIMING
    eTime = clock();
    Ctx->time[f] += (double)(eTime - sTime) / CLOCKS_PER_SEC;
    OP<<"Time for FuncAnalysis = "<<Ctx->time[f]<<"\n";
#endif
    return false;
}

bool FuncAnalysis::run()
{
#ifdef TIMING
    clock_t sTime, eTime;
    sTime = clock();
#endif
    buildPtsGraph();
#ifdef TIMING
    eTime = clock();
    OP<<"Time for buildPtsGraph: "<<(double)(eTime - sTime) / CLOCKS_PER_SEC<<"\n";
#endif
    //OP<<"1numNodes = "<<nodeFactory.getNumNodes()<<"\n";
    //sTime = clock();
    initSummary();
    //eTime = clock();
    //OP<<"time for initSummary "<<(double)(eTime - sTime) / CLOCKS_PER_SEC<<"\n";
    //OP<<"2numNodes = "<<nodeFactory.getNumNodes()<<"\n";
#ifdef TIMING
    sTime = clock();
#endif
    computeAASet();
#ifdef TIMING
    eTime = clock();
    OP<<"Time for computeAASet "<<(double)(eTime - sTime) / CLOCKS_PER_SEC<<"\n";

    //OP<<"3numNodes = "<<nodeFactory.getNumNodes()<<"\n";
    sTime = clock();
#endif
    qualiInference();
#ifdef TIMING
    eTime = clock();
    OP<<"Time for qualiInference "<<(double)(eTime - sTime) / CLOCKS_PER_SEC<<"\n";
#endif
    NodeIndex numNodes = nodeFactory.getNumNodes();
#ifdef CAL_STACKVAR
    calStackVar();
#endif
    //OP << "inference end.\n";
    //OP<<"4numNodes = "<<nodeFactory.getNumNodes()<<"\n";
    //errs()<<"fSummary for function "<<F->getName().str()<<"\n";
    //fSummary.dump();
    //OP<<"copy summraies for function "<<F<<": "<<F->getName().str()<<"\n";
    //fSummary.dump();
    //fSummary.summary();
    fSummary.copySummary(Ctx->FSummaries[F], fSummary, F);
    //OP<<"copy Summaries for function "<<F<<", name:"<<F->getName().str()<<"\n";
    //OP<<"retOffset = "<<Ctx->FSummaries[F].getRetOffset()<<"\n";
    std::string FScopeName = getScopeName(F, Ctx->funcToMd, Ctx->Funcs);
#ifdef SERIALIZATION
    std::string sFile = Ctx->outFolder + "/Summary/" + FScopeName;
    errs()<<sFile<<"\n";
    if (Ctx->incAnalysis)
    {
	size_t pos = FScopeName.find(Ctx->oldVersion);
        if (pos  != std::string::npos) {
            FScopeName.replace(pos, Ctx->oldVersion.size(), Ctx->newVersion);
        }	
        sFile = Ctx->incFolder+ "/Summary/" + FScopeName;
    }

    if (printWarning)
    {
        sFile += ".sum";
    }
    else
    {
        sFile += ".isum";
    }
    //std::string sFile = "sFile.txt";
#ifdef OP_DBG
    OP << ">>Serialize new summary to disk: " << sFile << "\n";
    //std::string testFile = "sFile.txt";
#endif
    std::ofstream ofile(sFile);
    boost::archive::text_oarchive oa(ofile);
    oa << Ctx->FSummaries[F];
    ofile.close();
#endif
#ifdef _PRINT_SUMMARY
    errs() << "Summary for function " << F->getName().str() << "\n";
    Ctx->FSummaries[F].summary();
//for (int i = 0; i < Ctx->FSummaries[F].sumNodeFactory.getNumNodes(); i++)
//Ctx->FSummaries[F].sumNodeFactory.dumpNode(i);
#endif

    /*If the flag is true, then we output the warnings to json,
    otherwise the record the result in fToWarns*/
    QualifierCheck();
#ifdef OOB_IO
    /*if (flag)
    {
        sTime = clock();
        QualifierCheck();
        eTime = clock();
        OP<<"time for QualifierCheck "<<(double)(eTime - sTime) / CLOCKS_PER_SEC<<"\n";
        Ctx->uninitArg += getUninitArg();
        Ctx->uninitOutArg += getUninitOutArg();
        //update = -1
        Ctx->ignoreOutArg += getIgnoreOutArg();
        OP << "Till now, Ctx->uninitArg = " << Ctx->uninitArg << ", Ctx->uninitOutArg = " << Ctx->uninitOutArg << ", Ctx->ignoreOutArg = " << Ctx->ignoreOutArg << "\n";
    }*/
    llvm::StringRef FName = F->getName();
    if (FName.startswith("SyS_"))
	FName = StringRef("sys_" + FName.str().substr(4));
    printCallees(F);
    initIOSummary();
    ioQualiInference();
    //errs() << "IOSummary for function " << F->getName().str() << ", addr :"<<F<<"\n";
    iofSummary.copySummary(Ctx->IOFSummaries[F], iofSummary, F);
    //Ctx->IOFSummaries[F].summary();
#ifdef SERIALIZATION
    sFile = Ctx->outFolder + "/IOSummary/"  + FScopeName;
    if (Ctx->incAnalysis)
    {
	    size_t pos = FScopeName.find(Ctx->oldVersion);
        if (pos  != std::string::npos) {
            FScopeName.replace(pos, Ctx->oldVersion.size(), Ctx->newVersion);
        }
        sFile = Ctx->incFolder + "/IOSummary/" + FScopeName;
    }

    if (printWarning)
    {
        sFile += ".iosum";
    }
    else
    {
        sFile += ".iiosum";
    }
    //std::string sFile = "sFile.txt";
    OP << ">>Serialize new iosummary to disk: " << sFile << "\n";
    //std::string testFile = "sFile.txt";
    std::ofstream ioofile(sFile);
    boost::archive::text_oarchive iooa(ioofile);
    iooa << Ctx->IOFSummaries[F];
    ioofile.close();
#endif
    
    if (Ctx->SyscallFuncs.count(FName.str()))
        ioQualifierCheck();
    printCallees(F);
    initOOBSummary();
    oobQualiInference();
    //errs() << "OOB Summary for function " << F->getName().str() << "\n";
    //oobfSummary.summary();
    oobfSummary.copySummary(Ctx->OOBFSummaries[F], oobfSummary, F);
#ifdef SERIALIZATION
    sFile = Ctx->outFolder + "/OOBSummary/"  + FScopeName;
    if (Ctx->incAnalysis)
    {
	    size_t pos = FScopeName.find(Ctx->oldVersion);
        if (pos  != std::string::npos) {
            FScopeName.replace(pos, Ctx->oldVersion.size(), Ctx->newVersion);
        }
        sFile = Ctx->incFolder + "/OOBSummary/" + FScopeName;
    }

    if (printWarning)
    {
        sFile += ".oobsum";
    }
    else
    {
        sFile += ".ioobsum";
    }
    //std::string sFile = "sFile.txt";
    OP << ">>Serialize new oobsummary to disk: " << sFile << "\n";
    //std::string testFile = "sFile.txt";
    std::ofstream oobofile(sFile);
    boost::archive::text_oarchive ooboa(oobofile);
    ooboa << Ctx->OOBFSummaries[F];
    oobofile.close();
#endif
    if (Ctx->SyscallFuncs.count(FName.str()))
        oobQualifierCheck();
#endif 
    return false;
}
void FuncAnalysis::buildPtsGraph()
{
    OP << "\n==========Function " << Ctx->order++ << ": " <<F<<" : "<< F->getName().str() << "==========\n";
    OP << "Module: " << Ctx->funcToMd[F] << "\n";
#ifdef DEBUG_TITLE
    OP << "Inside BuildPtsGraph:\n";
#endif
    PtsGraph initPtsGraph;
    createInitNodes(initPtsGraph);
#ifdef __PRINT_SUMNODEFACT
    OP << "sumNodeFactory:\n";
    for (int i = 0; i < fSummary.sumNodeFactory.getNumNodes(); i++)
        fSummary.sumNodeFactory.dumpNode(i);
#endif

#ifdef _INIT_PTS
    OP << "init ptsGraph:\n";
    for (auto i : initPtsGraph)
    {
        OP << "index " << i.first << " point to <";
        for (auto it : i.second)
            OP << it << ", ";
        OP << ">\n";
    }
#endif
#ifdef _INIT_NF
    OP << "init nodeFactory:\n";
    for (int i = 0; i < nodeFactory.getNumNodes(); i++)
    {
        nodeFactory.dumpNode(i);
    }
#endif
    for (Function::iterator bb = F->begin(); bb != F->end(); ++bb)
        for (BasicBlock::iterator itr = bb->begin(); itr != bb->end(); itr++)
        {
            NodeIndex idx = nodeFactory.createValueNode(&*itr);
        }
    /*
    for (inst_iterator itr  = inst_begin(*F), ite = inst_end(*F); itr != ite; ++itr) {
        OP<<"create value node for ins: "<<*itr<<"\n";
        nodeFactory.createValueNode(&*itr);
    }
    */
    std::deque<BasicBlock *> worklist;
    for (Function::iterator bb = F->begin(), be = F->end(); bb != be; ++bb)
    {
        worklist.push_back(&*bb);
    }

    Instruction *entry = &(F->front().front());
    NodeIndex entryIndex = nodeFactory.getValueNodeFor(entry);
    //Instruction *entry = &(F->front().front());
    //NodeIndex entryIndex = nodeFactory.getValueNodeFor(entry);
    unsigned count = 0, ptsIns = 0;
    unsigned threshold = 20 * worklist.size();
    
    while (!worklist.empty() && ++count <= threshold)
    {
        BasicBlock *BB = worklist.front();
        worklist.pop_front();

        PtsGraph in;
        PtsGraph out;

        // calculates BB in
        if (BB == &F->front())
        {
            in = initPtsGraph;
        }
        else
        {
            for (auto pi = pred_begin(BB), pe = pred_end(BB); pi != pe; ++pi)
            {
                const BasicBlock *pred = *pi;
                const Instruction *b = &pred->back();
                unionPtsGraph(in, nPtsGraph[b]);
            }
        }

        bool changed = false;
        for (BasicBlock::iterator ii = BB->begin(), ie = BB->end(); ii != ie; ++ii)
        {
            Instruction *I = &*ii;
            if (I != &BB->front())
                in = nPtsGraph[I->getPrevNode()];

#ifdef _PRINT_INST
            OP << *I << "\n";
#endif
#ifdef IN
            OP << "===========IN===========\n";
            for (auto i : in)
            {
                //if(i.first < entryIndex && i.first>nodeFactory.getNullObjectNode())
                //  continue;
                OP << "index " << i.first << " point to <";
                for (auto it : i.second)
                    OP << it << ", ";
                OP << ">\n";
            }
#endif
            out = processInstruction(I, in);
	    ptsIns++;
            if (I == &BB->back())
            {
                PtsGraph old = nPtsGraph[I];
                if (out != old)
                    changed = true;
            }
            nPtsGraph[I] = out;

#ifdef OUT
            OP << "===========OUT===========\n";
            for (auto i : out)
            {
                //if(i.first < entryIndex && i.first>nodeFactory.getNullObjectNode())
                //    continue;
                if (i.second == in[i.first])
                    continue;
                OP << "index " << i.first << " point to <";
                for (auto it : i.second)
                    OP << it << ", ";
                OP << ">\n";
            }
#endif
        }

        if (changed)
        {
            for (auto si = succ_begin(BB), se = succ_end(BB); si != se; ++si)
            {
                BasicBlock *succ = *si;
                if (std::find(worklist.begin(), worklist.end(), succ) == worklist.end())
                    worklist.push_back(succ);
            }
        }
    } //worklist
    //OP<<"==stat: ptsGraph BB: "<<count<<", ptsIns = "<<ptsIns<<"\n";
    Instruction *endIns = &(F->back().back());
    //OP<<"---node factory size = "<<nodeFactory.getNumNodes()<<"\n";
#ifdef _PRINT_NODEFACTORY
    OP << "NodeFactory:\n";
    for (int i = 0; i < nodeFactory.getNumNodes(); i++)
        nodeFactory.dumpNode(i);
#endif

#ifdef _PRINT_PTS
    OP << "The ptsGraph at the final node:\n";
    for (auto i : nPtsGraph[endIns])
    {
        OP << "index " << i.first << " point to <";
        for (auto it : i.second)
            OP << it << ", ";
        OP << ">\n";
    }
#endif
}
void FuncAnalysis::handleGEPConstant(const ConstantExpr *ce, PtsGraph &in)
{
    NodeIndex gepIndex = nodeFactory.getValueNodeFor(ce);
    NodeIndex srcNode = nodeFactory.getValueNodeFor(ce->getOperand(0));
    int64_t offset = getGEPOffset(ce, DL);
    int fieldNum = 0;
    if (offset < 0)
    {
        //OP<<"offset = "<<offset<<"\n";
        // update the out pts graph instead of the in
        //<= functon: acpi_ns_validate_handle: %7 = icmp eq i8* %6, getelementptr (i8, i8* null, i64 -1)
        if (ce->getOperand(0)->isNullValue())
            return;

        //if(cast<PointerType>(ce->getType())->getElementType()->isIntegerTy(8))
        //fieldNum = handleContainerOf(ce, offset, srcNode, in);
        // XXX: negative encoded as unsigned
    }
    else
    {
        fieldNum = offsetToFieldNum(ce->getOperand(0)->stripPointerCasts(), offset, DL,
                                    &Ctx->structAnalyzer, M);
    }

    //in[gepIndex].insert(nodeFactory.getObjNodeForGEPExpr(gepIndex));

    for (auto n : in[srcNode])
    {
        //assert(nodeFactory.isObjectNode(n) && "src ptr does not point to obj node");
        //if (!nodeFactory.isObjectNode(n))
        if (n > nodeFactory.getConstantIntNode() && !nodeFactory.isUnOrArrObjNode(n))
            in[gepIndex].insert(n + fieldNum);
        else
            in[gepIndex].insert(n);
    }
}
PtsGraph FuncAnalysis::processInstruction(Instruction *I, PtsGraph &in)
{

    // handle constant GEPExpr oprands here
    for (auto const &opr : I->operands())
    {
        const Value *v = opr.get();
        if (const ConstantExpr *ce = dyn_cast<ConstantExpr>(v))
        {
            if (ce->getOpcode() == Instruction::GetElementPtr)
            {
                NodeIndex gepIndex = nodeFactory.getValueNodeFor(ce);
                NodeIndex srcNode = nodeFactory.getValueNodeFor(ce->getOperand(0));
                int64_t offset = getGEPOffset(ce, DL);
                int fieldNum = 0;
                if (offset < 0)
                {
                    // update the out pts graph instead of the in
                    //<= functon: acpi_ns_validate_handle: %7 = icmp eq i8* %6, getelementptr (i8, i8* null, i64 -1)
                    if (ce->getOperand(0)->isNullValue())
                        continue;

                    if (I->getType()->isPointerTy() && cast<PointerType>(I->getType())->getElementType()->isIntegerTy(8))
                        fieldNum = handleContainerOf(I, offset, srcNode, in);
                    // XXX: negative encoded as unsigned
                }
                else
                {
                    fieldNum = offsetToFieldNum(ce->getOperand(0)->stripPointerCasts(), offset, DL,
                                                &Ctx->structAnalyzer, M);
                }

                //in[gepIndex].insert(nodeFactory.getObjNodeForGEPExpr(gepIndex));

                for (auto n : in[srcNode])
                {
                    //assert(nodeFactory.isObjectNode(n) && "src ptr does not point to obj node");
                    //if (!nodeFactory.isObjectNode(n))
                    if (n > nodeFactory.getConstantIntNode() && !nodeFactory.isUnOrArrObjNode(n))
                        in[gepIndex].insert(n + fieldNum);
                    else
                        in[gepIndex].insert(n);
                }
            }
            if (ce->getOpcode() == Instruction::BitCast)
            {
                if (const ConstantExpr *cep = dyn_cast<ConstantExpr>(ce->getOperand(0)))
                {
                    if (cep->getOpcode() == Instruction::GetElementPtr)
                    {
                        handleGEPConstant(cep, in);
                    }
                }
            }
        }
    }

    PtsGraph out = in;
    switch (I->getOpcode())
    {
    case Instruction::Alloca:
    {
        NodeIndex valNode = nodeFactory.getValueNodeFor(I);
        assert(valNode != AndersNodeFactory::InvalidIndex && "Failed to find alloca value node");
        assert(I->getType()->isPointerTy());
        createNodeForPointerVal(I, I->getType(), valNode, out);
        //cannot call the function createNodeForPointer, because the allocated
        break;
    }
    case Instruction::Load:
    {
        if (I->getType()->isPointerTy())
        {
            // val = load from op
            NodeIndex ptrNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(ptrNode != AndersNodeFactory::InvalidIndex && "Failed to find load operand node");
            NodeIndex valNode = nodeFactory.getValueNodeFor(I);
            assert(valNode != AndersNodeFactory::InvalidIndex && "Failed to find load value node");

            for (auto i : in[ptrNode])
            {
#ifdef DBG_LOAD
                OP << i << ":\n";
                for (auto item : in[i])
                {
                    OP << "item " << item << ", ";
                }
                OP << "\n";
#endif
                out[valNode].insert(in[i].begin(), in[i].end());
                //if the ins is a pointer but it points to nothing, then we put universal ptr node into it.
                if (out[valNode].isEmpty())
                {
                    out[valNode].insert(nodeFactory.getUniversalPtrNode());
                }
		if (nodeFactory.isUnOrArrObjNode(i)) {
		    //OP<<i<<" is union node.\n";
		    nodeFactory.setUnOrArrObjNode(valNode);
		    //OP<<"set "<<valNode<<" as union node.\n";
		}
            }
        }
        break;
    }
    case Instruction::Store:
    {
        NodeIndex dstNode = nodeFactory.getValueNodeFor(I->getOperand(1));
        assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find store dst node");
        NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
        if (srcNode == AndersNodeFactory::InvalidIndex)
        {
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

        if (in.find(srcNode) != in.end())
        {
            for (auto i : in[dstNode])
            {
                if (i <= nodeFactory.getConstantIntNode())
                    continue;
                out[i] = in[srcNode];
                if (nodeFactory.isArgNode(srcNode))
                {
                    //OP<<"set "<<i<<" as arg node.\n";
                    nodeFactory.setArgNode(i);
                }
            }
        }

        break;
    }
    case Instruction::GetElementPtr:
    {
        //OP << "NodeFactory:\n";
        //for (int i = 0; i < nodeFactory.getNumNodes(); i++)
        //nodeFactory.dumpNode(i);
        //dstNode = gep srcNode offset
        NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
        assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find gep src node");
        NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
        assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find gep dst node");
        //OP<<"srcNode = "<<srcNode<<", dstNode = "<<dstNode<<"\n";
        //OP << "Inside gep inst, before call the function getGepInstFieldNum:\n";
        //Ctx->structAnalyzer.printStructInfo();
        const GEPOperator *gepValue = dyn_cast<GEPOperator>(I);
        assert(gepValue != NULL && "getGEPOffset receives a non-gep value!");
#ifdef sum
        const Value *baseValue = gepValue->getPointerOperand()->stripPointerCasts();
        SmallVector<Value *, 4> indexOps(gepValue->op_begin() + 1, gepValue->op_end());
        for (unsigned i = 0, e = indexOps.size(); i != e; ++i)
        {
            if (!isa<ConstantInt>(indexOps[i]))
            {
                out[dstNode].insert(nodeFactory.getUniversalPtrNode());
                break;
            }
        }
#endif
        int64_t offset = getGEPOffset(I, DL);
        llvm::Type *srcType = I->getType();

        const Type *type = cast<PointerType>(srcType)->getElementType();
        // An array is considered a single variable of its type.
        while (const ArrayType *arrayType = dyn_cast<ArrayType>(type))
            type = arrayType->getElementType();

            //OP<<"offset = "<<offset<<"\n";
            //sequential gep inst, function pci_msi_prepare, we treat it as one field
            //To be removed
#ifdef removed
        if (isa<GetElementPtrInst>(I->getOperand(0)))
        {
            out[dstNode] = in[srcNode];
            break;
        }
#endif
        bool innerele = false;
        for (auto obj : in[srcNode])
        {
            //OP<<"1obj = "<<obj<<"\n";
            if (offset < 0 && in[obj].getSize() == 1)
            {
                for (auto inner : in[obj])
                {
                    if (inner <= nodeFactory.getConstantIntNode())
                        innerele = true;
                }
            }
        }

        if (innerele)
        {
            out[dstNode] = in[srcNode];
            break;
        }
        //#endif
        int fieldNum = 0;
        bool onlyUnion = true;
        //OP<<"offset = "<<offset<<"\n";
        if (offset < 0)
        {
            bool special = true;
            for (auto obj : out[srcNode])
            {
                //OP<<"2obj = "<<obj<<"\n";
                if (obj <= nodeFactory.getConstantIntNode())
                {
                    //OP<<"dstNode = "<<dstNode<<"\n";
                    out[dstNode].insert(obj);
                    continue;
                }
                else
                {
                    special = false;
                }
                // update the out pts graph instead of the in
            }
            if (!special)
            {

                //OP << "handle container:\n";
                if (cast<PointerType>(I->getType())->getElementType()->isIntegerTy(8))
                    fieldNum = handleContainerOf(I, offset, srcNode, out);
                //OP<<"fieldNum = "<<fieldNum<<"\n";
            }
            // XXX: negative encoded as unsigned
        }
        else
        {
	    bool isUnion = false;
            llvm::Type* srcTy= cast<PointerType>(I->getOperand(0)->getType())->getElementType();
            //OP<<"srcTy = "<<*srcTy<<"\n";
            for (User *U : I->users()) {
            	if (Instruction *Use = dyn_cast<Instruction>(U)) {
                    if (BitCastInst *BCInst = dyn_cast<BitCastInst>(Use)) {
                    	llvm::Type *srcType = BCInst->getSrcTy();
                        //OP<<"BCInst srcType = "<<*srcType<<"\n";
                        std::string insStr;
                        llvm::raw_string_ostream rso(insStr);
                        srcType->print(rso);
                        if (insStr.find("union") != std::string::npos) isUnion=true;
                    //if (srcType->isPointerTy() && cast<PointerType>(srcType)->getElementType()->isUnionType()) {
                        //isUnion = true;
                    //}
                    }
                }
            }
            /*for (auto obj : out[srcNode])
            {
		//OP<<"obj = "<<obj<<"\n";
                if (obj <= nodeFactory.getConstantIntNode())
                {
                    out[dstNode].insert(obj);
                    continue;
                }
                if (!nodeFactory.isUnOrArrObjNode(obj))
                {
                    onlyUnion = false;
                }
            }*/
            if (!isUnion)
            {
                //OP<<"type: "<<*I->getOperand(0)->getType()<<"\n";
                //OP<<"stripPtrCast:"<<*I->getOperand(0)->stripPointerCasts()<<"\n";
                fieldNum = offsetToFieldNum(I->getOperand(0)->stripPointerCasts(), offset, DL,
                                            &Ctx->structAnalyzer, M);
            }
            //OP<<"1fieldNum = "<<fieldNum<<"\n";
            //check if the offset of union, but I remember I change the function offsetToFieldNum,
            //so this could be removed?

            for (auto n : in[srcNode])
            {
#ifdef checkunion
                if (nodeFactory.isUnOrArrObjNode(n))
                {
                    out[dstNode].insert(n);
                    continue;
                }
#endif
                //OP<<"constantIndeNode = "<<nodeFactory.getConstantIntNode()<<"\n";
                if (n > nodeFactory.getConstantIntNode())
                {
                    //OP<<"first if.\n";
                    if (fieldNum <= nodeFactory.getObjectSize(n))
                    {
                        //OP<<"second if.\n";
                        unsigned bound = nodeFactory.getObjectBound(n);
                        //OP<<"bound = "<<bound<<"\n";
                        if (n + fieldNum < bound)
                            out[dstNode].insert(n + fieldNum);
                        else
                            out[dstNode].insert(n);
                    }
                    else
                        out[dstNode].insert(nodeFactory.getUniversalPtrNode());
                }
                else
                    out[dstNode].insert(n);
            }
            //OP<<"offset = "<<offset<<", fieldNum = "<<fieldNum<<"\n";
        }
        //OP<<"numnodes : "<<nodeFactory.getNumNodes()<<"\n";
        // since we may have updated the pts graph, we need to use the out instead of in

#ifdef gep_check
        OP << "srcIndex = " << srcIndex << "\n";
        OP << "out[" << dstIndex << "]:\n";
        for (auto pts : out[dstIndex])
            OP << pts << ", ";
        OP << "\n";
#endif
#ifdef print
        OP << "NodeFactory:\n";
        for (int i = 0; i < nodeFactory.getNumNodes(); i++)
            nodeFactory.dumpNode(i);
#endif
        break;
    }
    case Instruction::PHI:
    {
        const PHINode *PHI = cast<PHINode>(I);
        if (PHI->getType()->isPointerTy())
        {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(PHI);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find phi dst node");
            for (unsigned i = 0, e = PHI->getNumIncomingValues(); i != e; ++i)
            {
                NodeIndex srcNode = nodeFactory.getValueNodeFor(PHI->getIncomingValue(i));
                assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find phi src node");
                if (in.find(srcNode) != in.end())
                    out[dstNode].insert(in[srcNode].begin(), in[srcNode].end());
            }
        }
        break;
    }
    case Instruction::BitCast:
    {
        NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
        assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find bitcast dst node");
        NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
        assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find bitcast src node");
        //OP<<"dstNode = "<<dstNode<<", srcNode = "<<srcNode<<"\n";
        assert(I->getType()->isPointerTy());
        Type *T = cast<PointerType>(I->getType())->getElementType();
        Type *srcTy = cast<PointerType>(I->getOperand(0)->getType())->getElementType();


	if (nodeFactory.isUnOrArrObjNode(srcNode)) {
		nodeFactory.setUnOrArrObjNode(dstNode);
	}
        //do not cast a union obj
        bool unionType = false;
        for (auto obj : out[srcNode])
        {
            //OP<<"0obj = "<<obj<<"\n";
            if (obj <= nodeFactory.getConstantIntNode())
                continue;
            unsigned objSize = nodeFactory.getObjectSize(obj);
            //OP<<"objSize = "<<objSize<<"\n";
	    if (objSize == 1 && nodeFactory.isUnOrArrObjNode(obj))
            {
		//OP<<obj <<" is union type.\n";
                out[dstNode] = in[srcNode];
                unionType = true;
                break;
            }
        }
        if (unionType) {
	    for (auto obj : out[dstNode]) {
		//OP<<"out obj = "<<obj<<"\n";
		if (obj <= nodeFactory.getConstantIntNode())
			continue;
	    	nodeFactory.setUnOrArrObjNode(obj);
	    }
            break;
	}
        if (StructType *srcST = dyn_cast<StructType>(srcTy))
        {
            if (!srcST->isLiteral() && srcST->getStructName().startswith("union"))
            {
                out[dstNode] = in[srcNode];
                break;
            }
        }
        if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(I->getOperand(0)))
        {

            if (getGEPOffset(GEPI, DL) != 0)
            {
                out[dstNode] = in[srcNode];
                break;
            }
        }
        //OP<<"1\n";
        if (StructType *ST = dyn_cast<StructType>(T))
        {
            if (ST->isOpaque())
            {
                out[dstNode] = out[srcNode];
                break;
            }
            const StructInfo *stInfo = Ctx->structAnalyzer.getStructInfo(ST, M);
            assert(stInfo != NULL && "Failed to find stinfo");

            unsigned stSize = stInfo->getExpandedSize();
            //OP<<"stSize = "<<stSize<<"\n";
            //disable assertion: retrieve_io_and_region_from_bio
            //assert(!in[srcNode].isEmpty());
            for (NodeIndex obj : in[srcNode])
            {
                if (obj <= nodeFactory.getConstantIntNode())
                    continue;
                //special case: gep and then cast, <=Function get_dx_countlimit
                if (!nodeFactory.isObjectNode(obj))
                    continue;
                unsigned objSize = nodeFactory.getObjectSize(obj);
                //OP<<"1obj = "<<obj<<", objSize = "<<objSize<<"\n";
                if (stSize > objSize)
                {
#if 0
                        OP << "BitCast: " << *I << ":"
                           << " dst size " << stSize
                           << " larger than src size " << objSize << "\n";
#endif
                    NodeIndex newObj = extendObjectSize(obj, ST, out);
                    unsigned newOffset = nodeFactory.getObjectOffset(newObj);
                    //if the object is pointed in the middle, then load will cause problems, we need an assertion to promise the
                    //object is not read.
                    if (newObj == obj)
                    {
                        for (User *U : I->users())
                        {
                            if (Instruction *Use = dyn_cast<Instruction>(U))
                            {

                                if (isa<LoadInst>(Use))
                                {
                                    OP << "Use: " << *Use << "\n";
                                    assert(0 && "Casting struct get used.");
                                    OP << "Warning: Casting struct get used.\n";
                                }
                                if (isa<GetElementPtrInst>(Use))
                                {
                                    GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(Use);
                                    int64_t off = getGEPOffset(GEPI, DL);
                                    if (off == 0)
                                        continue;
                                    //OP<<"GEPUse: "<<*Use<<"\n";
                                    for (User *GEPU : Use->users())
                                    {
                                        if (Instruction *GepUse = dyn_cast<Instruction>(GEPU))
                                        {
                                            if (isa<LoadInst>(GepUse))
                                            {
                                                OP << "Use: " << *GepUse << "\n";
                                                //assert(0 && "Casting struct get used.");
                                                OP << "Warning: Casting struct get used.\n";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    //OP<<"2\n";
                    if (newObj != obj && nodeFactory.isArgNode(obj))
                    {
                        for (unsigned i = 0; i < stSize; i++)
                        {
                            nodeFactory.setArgNode(newObj - newOffset + i);
                        }
                    }
                    //OP<<"3\n";
                    if (newObj != obj && nodeFactory.isHeapNode(obj))
                    {
                        for (unsigned i = 0; i < stSize; i++)
                        {
                            nodeFactory.setHeapNode(newObj - newOffset + i);
                        }
                    }
                }
                //the obj could not be the first element of the obj
                else
                {
                    unsigned newOffset = nodeFactory.getObjectOffset(obj);
                    //OP<<"newOffset = "<<newOffset<<"\n";
                    for (unsigned i = 0; i < stSize; i++)
                    {
                        //OP<<"i = "<<i<<", obj - newOffset + i = "<<obj - newOffset + i<<"\n";
                        if (stInfo->isFieldUnion(i))
                            nodeFactory.setUnOrArrObjNode(obj - newOffset + i);
                    }
                    if (nodeFactory.isHeapNode(obj))
                    {
                        for (unsigned i = 0; i < stSize; i++)
                        {
                            nodeFactory.setHeapNode(obj - newOffset + i);
                        }
                    }
                }
            }
        }
        // since we may have updated the pts graph, we need to use the out instead of in
        out[dstNode] = out[srcNode];
        //OP<<"end of bitcast.";
        break;
    }
    case Instruction::Trunc:
    {

        if (I->getType()->isPointerTy())
        {
            NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
            assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find trunc src node");
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find trunc dst node");
            if (nodeFactory.isUnOrArrObjNode(srcNode)) {
                nodeFactory.setUnOrArrObjNode(dstNode);
            }
	    if (in.find(srcNode) != in.end())
                out[dstNode] = in[srcNode];
        }
        break;
    }
    case Instruction::IntToPtr:
    {
        // Get the node index for dst
        NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
        NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
        assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr dst node");
        assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr src node");

	if (nodeFactory.isUnOrArrObjNode(srcNode)) {
                nodeFactory.setUnOrArrObjNode(dstNode);
        }
        // ptr to int to ptr
        if (in.find(srcNode) != in.end() && !in[srcNode].isEmpty())
            out[dstNode] = in[srcNode];
        else
            out[dstNode].insert(nodeFactory.getUniversalPtrNode());
        break;
    }
    // FIXME point arithmetic through int
    case Instruction::PtrToInt:
    {
        // Get the node index for dst
        NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
        NodeIndex srcNode = nodeFactory.getValueNodeFor(I->getOperand(0));
        assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr dst node");
        assert(srcNode != AndersNodeFactory::InvalidIndex && "Failed to find inttoptr src node");

	if (nodeFactory.isUnOrArrObjNode(srcNode)) {
            nodeFactory.setUnOrArrObjNode(dstNode);
        }
        out[dstNode] = in[srcNode];
        break;
    }
    //case Instruction::BinaryOps: {
    //    break;
    //}
    case Instruction::Select:
    {
        out = in;
        if (I->getType()->isPointerTy())
        {
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
        }
        break;
    }
    case Instruction::VAArg:
    {
        if (I->getType()->isPointerTy())
        {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find va_arg dst node");
            NodeIndex vaNode = nodeFactory.getVarargNodeFor(I->getParent()->getParent());
            assert(vaNode != AndersNodeFactory::InvalidIndex && "Failed to find va_arg node");
            if (in.find(vaNode) != in.end())
                out[dstNode] = in[vaNode];
        }
        break;
    }
    case Instruction::Call:
    {
        CallInst *CI = dyn_cast<CallInst>(I);
        if (CI->getCalledFunction())
        {
            std::string CFN = CI->getCalledFunction()->getName().str();
            //OP<<"Callee: "<<CFN<<"\n";
        }
        if (I->getType()->isPointerTy())
        {
            NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
            assert(dstNode != AndersNodeFactory::InvalidIndex && "Failed to find call dst node");
            out[dstNode].clear();
            bool init = false;
            if (CI->getCalledFunction())
            {
                std::string CFN = getScopeName(CI->getCalledFunction());
                //OP<<"Callee: "<<CFN<<"\n";
            }
#ifdef sep
            if (CI->getCalledFunction())
            {
                StringRef fName = CI->getCalledFunction()->getName();
                createNodeForPointerVal(I, I->getType(), dstNode, out);
                if (Ctx->HeapAllocFuncs.count(fName.str()))
                {
                    NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
                    for (auto obj : out[dstNode])
                    {
                        if (obj <= nodeFactory.getConstantIntNode())
                            continue;
                        unsigned objSize = nodeFactory.getObjectSize(obj);
                        for (unsigned i = 0; i < objSize; i++)
                        {
                            nodeFactory.setHeapNode(obj + i);
                        }
                    }
                }
                else if (Ctx->CopyFuncs.count(fName.str()))
                {
                    NodeIndex dstIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(0));
                    NodeIndex srcIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(1));
                    if (CI->getArgOperand(0)->getType()->isPointerTy() && CI->getArgOperand(1)->getType()->isPointerTy())
                    {
                        for (auto srcObj : out[srcIndex])
                        {
                            for (auto dstObj : out[dstIndex])
                            {
                                out[dstObj].insert(out[srcObj].begin(), out[srcObj].end());
                            }
                        }
                    }
                }
            }
            else
            {
#endif
                for (auto func : Ctx->Callees[CI])
                {
                    StringRef fName = func->getName();
                    if (!init)
                    {
                        createNodeForPointerVal(I, I->getType(), dstNode, out);
                        init = true;
                    }

                    if (Ctx->HeapAllocFuncs.count(fName.str()))
                    {
                        NodeIndex dstNode = nodeFactory.getValueNodeFor(I);
                        for (auto obj : out[dstNode])
                        {
                            if (obj <= nodeFactory.getConstantIntNode())
                                continue;
                            unsigned objSize = nodeFactory.getObjectSize(obj);
                            for (unsigned i = 0; i < objSize; i++)
                            {
                                nodeFactory.setHeapNode(obj + i);
                                //OP<<"obj + i"<<obj + i<<" is a heap node.\n";
                            }
                        }
                    }
                    else if (Ctx->CopyFuncs.count(fName.str()))
                    {
                        if (CI->getNumArgOperands() < 3)
                            break;
                        if (!CI->getArgOperand(0)->getType()->isPointerTy() || !CI->getArgOperand(1)->getType()->isPointerTy())
                            break;
                        NodeIndex dstIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(0));
                        NodeIndex srcIndex = nodeFactory.getValueNodeFor(CI->getArgOperand(1));
                        if (CI->getArgOperand(0)->getType()->isPointerTy() && CI->getArgOperand(1)->getType()->isPointerTy())
                        {
                            for (auto srcObj : out[srcIndex])
                            {
                                for (auto dstObj : out[dstIndex])
                                {
                                    out[dstObj].insert(out[srcObj].begin(), out[srcObj].end());
                                }
                            }
                        }
                    }
                }
                // } //endif
            }
            break;
        }
    case Instruction::Ret:
    {
        if (I->getNumOperands() != 0)
        {
            Value *RV = I->getOperand(0);
            NodeIndex valNode = nodeFactory.getValueNodeFor(RV);
            assert(valNode != AndersNodeFactory::InvalidIndex && "Failed to find call return value node");
            if (RV->getType()->isPointerTy())
            {
                NodeIndex retNode = nodeFactory.getReturnNodeFor(F);
                assert(retNode != AndersNodeFactory::InvalidIndex && "Failed to find call return node");
                out[retNode] = in[valNode];
            }
            else
            {
                if (!isa<ConstantInt>(RV))
                {
                    auto itr = in.find(valNode);
                    //Exception: ptrtoint
                    //assert(itr == in.end() || itr->second.isEmpty() ||
                    //  (itr->second.getSize() == 1 &&
                    //  itr->second.has(nodeFactory.getConstantIntNode())
                    // )
                    //);
                }
            }
        }
        break;
    }
    default:
    {
        if (I->getType()->isPointerTy())
            OP << "pointer-related inst not handled!" << *I << "\n";
        //WARNING("pointer-related inst not handled!" << *I << "\n");
        //assert(!inst->getType()->isPointerTy() && "pointer-related inst not handled!");
        out = in;
        break;
    }
    } //switch(I->getOpCode())
        return out;
    }

    void FuncAnalysis::unionPtsGraph(PtsGraph & pg, const PtsGraph &other)
    {
        for (auto itr2 = other.begin(), end2 = other.end(); itr2 != end2; ++itr2)
        {
            auto itr1 = pg.find(itr2->first);
            if (itr1 != pg.end())
                itr1->second.insert(itr2->second.begin(), itr2->second.end());
            else
                pg.insert(std::make_pair(itr2->first, itr2->second));
        }
    }

    void FuncAnalysis::initSummary()
    {
        fSummary.relatedBC.insert(F->getParent()->getName().str());
        //retValue and argValue node are created in: createInitNodes
        Instruction *beginIns = &(F->front().front());
        Instruction *endIns = &(F->back().back());
        //1-1. create return value node and argument value node
        NodeIndex sumRetNode = fSummary.sumNodeFactory.getValueNodeFor(F);
        NodeIndex retNode = nodeFactory.getReturnNodeFor(F);
        //OP<<"return type: "<<*F->getReturnType()<<"\n";
        //1-2 create return object node
        if (F->getReturnType()->isVoidTy()) {
            fSummary.sumPtsGraph[sumRetNode].clear();
        }
        else {
            //create node and build the pts relations of return object
	        Type *type = F->getReturnType();
	        Type *eleType = NULL;
	        unsigned objSize = 0;
    	    if (!type->isVoidTy()) {
        	    if (type->isPointerTy()) {
		            eleType = cast<PointerType>(type)->getElementType();
	    	        while (const ArrayType *arrayType= dyn_cast<ArrayType>(eleType))
        		        eleType = arrayType->getElementType();
	    	        if (const StructType *structType = dyn_cast<StructType>(eleType)) {
			            if(structType->isOpaque()) {
				            objSize = 1;
			            }
			            else if(!structType->isLiteral() && structType->getStructName().startswith("union")) {
				            objSize = 1;
			            }
			            else {
				            const StructInfo* stInfo = Ctx->structAnalyzer.getStructInfo(structType, M);
				            objSize = stInfo->getExpandedSize();
			            }
	    	        }
	    	        else {
			            objSize = 1;	
		            }
                    
		        }
	        }
	    for (auto obj : nPtsGraph[endIns][retNode])
            {
                if (obj <= nodeFactory.getConstantIntNode())
                    continue;
                if (fSummary.sumNodeFactory.getObjectNodeFor(F) != AndersNodeFactory::InvalidIndex)
                    break;
                NodeIndex sumRetObjNode = fSummary.sumNodeFactory.createObjectNode(F);
		fSummary.setRetSize(objSize);
                //OP<<"sumRetNode = "<<sumRetNode<<", objSize = "<<objSize<<"\n";
                //OP<<"offset = "<<nodeFactory.getObjectOffset(obj)<<"\n";
                if (nodeFactory.getObjectOffset(obj) == 0)
                {
                    fSummary.sumPtsGraph[sumRetNode].insert(sumRetObjNode);
                    fSummary.setRetOffset(0);
                }
                for (unsigned i = 1; i < objSize; i++)
                {
                    NodeIndex sumObjNode = fSummary.sumNodeFactory.createObjectNode(sumRetObjNode, i);
                    if (i == nodeFactory.getObjectOffset(obj))
                    {
                        fSummary.sumPtsGraph[sumRetNode].insert(sumObjNode);
                        fSummary.setRetOffset(i);
                    }
                }
            }
        }
        //OP<<"create object node for args.\n";
        //1-3 create argument object node
        int argNo = 0;
        for (auto const &a : F->args())
        {
            const Argument *arg = &a;
            NodeIndex argNode = nodeFactory.getValueNodeFor(arg);
            NodeIndex sumArgNode = fSummary.sumNodeFactory.getValueNodeFor(arg);

            fSummary.args[sumArgNode].setNodeIndex(sumArgNode);
            fSummary.args[sumArgNode].setNodeArgNo(argNo);
            //OP<<"sumArgNode = "<<sumArgNode<<", argNode = "<<argNode<<"\n";;
            //create node for object, set the sumPts information
            //create the array for update and requirement of the function
            for (auto obj : nPtsGraph[endIns][argNode])
            {
                if (obj <= nodeFactory.getConstantIntNode())
                    continue;
                unsigned objSize = nodeFactory.getObjectSize(obj);
                NodeIndex sumArgObjNode = fSummary.sumNodeFactory.getObjectNodeFor(arg);
                if (sumArgObjNode != AndersNodeFactory::InvalidIndex)
                    break;
                fSummary.args[argNo].setObjSize(objSize);
                sumArgObjNode = fSummary.sumNodeFactory.createObjectNode(arg);
                //OP<<"sumArgObjNode = "<<sumArgObjNode<<"\n";
                if (nodeFactory.getObjectOffset(obj) == 0)
                {
                    fSummary.sumPtsGraph[sumArgNode].insert(sumArgObjNode);
                    fSummary.args[sumArgObjNode].setNodeArgNo(argNo);
                    fSummary.args[sumArgObjNode].setOffset(0);
                }
                for (unsigned i = 1; i < objSize; i++)
                {
                    NodeIndex objNode = fSummary.sumNodeFactory.createObjectNode(sumArgObjNode, i);
                    fSummary.args[objNode].setNodeArgNo(argNo);
                    if (i == nodeFactory.getObjectOffset(obj))
                    {
                        fSummary.sumPtsGraph[sumArgNode].insert(objNode);
                        fSummary.args[argNo].setOffset(i);
                    }
                }
            }
            argNo++;
        }
#ifdef _PRINT_SUMNODEFACT
        OP << "sumNodeFactory:\n";
        for (int i = 0; i < fSummary.sumNodeFactory.getNumNodes(); i++)
            fSummary.sumNodeFactory.dumpNode(i);
        OP << "sumPtsGraph:\n";
        for (auto i : fSummary.sumPtsGraph)
        {
            OP << "ptr: " << i.first << "->";
            for (auto o : i.second)
            {
                OP << o << "\n";
            }
        }
#endif
        //2. create the summary array for both update and requirments
        unsigned numNodes = fSummary.sumNodeFactory.getNumNodes();
        fSummary.noNodes = numNodes;
        fSummary.reqVec.resize(numNodes);    //= new int[numNodes];
        fSummary.updateVec.resize(numNodes); //= new int[numNodes];
        fSummary.changeVec.resize(numNodes);
        for (unsigned i = 0; i < numNodes; i++)
        {
            fSummary.reqVec.at(i) = _UNKNOWN;
            fSummary.updateVec.at(i) = _UNKNOWN;
            fSummary.changeVec.at(i) = _UNKNOWN;
        }
    }
    // given old object node and new struct info, extend the object size
    // return the new object node
    NodeIndex FuncAnalysis::extendObjectSize(NodeIndex oldObj, const StructType *stType, PtsGraph &ptsGraph)
    {
        //OP<<"inside extendobjsize:\n";
        // FIXME: assuming oldObj is the base <= function: acpi_dev_filter_resource_type
        //disable assertion:  function: acpi_dev_filter_resource_type
        //assert(nodeFactory.getObjectOffset(oldObj) == 0);
        if (nodeFactory.getObjectOffset(oldObj) != 0)
            return oldObj;

        const Value *val = nodeFactory.getValueForNode(oldObj);
        assert(val != nullptr && "Failed to find value of node");
        NodeIndex valNode = nodeFactory.getValueNodeFor(val);

        // before creating new obj, remove the old ptr->obj
        auto itr = ptsGraph.find(valNode);
        //function tcp_prune_ofo_queue
        assert(itr != ptsGraph.end());
        if (!itr->second.has(oldObj))
        {
            OP << "valNode does not own the oldObj.";
        }
        itr->second.reset(oldObj);
        nodeFactory.removeNodeForObject(val);

        // create new obj
        NodeIndex newObj = processStruct(val, stType, valNode, ptsGraph);
        //set ArgNode:
        if (nodeFactory.isArgNode(oldObj))
        {
            unsigned stSize = nodeFactory.getObjectSize(newObj);
            unsigned newOffset = nodeFactory.getObjectOffset(newObj);
            for (unsigned i = 0; i < stSize; i++)
                nodeFactory.setArgNode(newObj - newOffset + i);
        }
        // update ptr2set. all the pointers to oldObj should to new Obj
        updateObjectNode(oldObj, newObj, ptsGraph);

        return newObj;
    }

    // given the old object node, old size, and the new object node
    // replace all point-to info to the new node
    // including the value to object node mapping
    void FuncAnalysis::updateObjectNode(NodeIndex oldObj, NodeIndex newObj, PtsGraph & ptsGraph)
    {
        //OP<<"Inside updateObjectNode:\n";
        unsigned offset = nodeFactory.getObjectOffset(oldObj);
        NodeIndex baseObj = nodeFactory.getOffsetObjectNode(oldObj, -(int)offset);
        unsigned size = nodeFactory.getObjectSize(oldObj);
        //OP<<"offset = "<<offset<<", baseObj = "<<baseObj<<", newObj = "<<newObj<<", size = "<<size<<"\n";
        //OP<<"1.\n";
        // well, really expensive op
        // use explicit iterator for updating
        for (auto itr = ptsGraph.begin(), end = ptsGraph.end(); itr != end; ++itr)
        {
            AndersPtsSet temp = itr->second;
            // in case modification will break iteration
            for (NodeIndex obj : temp)
            {
                //if(nodeFactory.getOffset(obj) == 0)
                //continue;
                if (obj >= baseObj && obj < (baseObj + size))
                {
                    itr->second.reset(obj);
                    itr->second.insert(newObj + (obj - baseObj));
                }
           }
        }
    }

    int FuncAnalysis::handleContainerOf(const Instruction *I, int64_t offset, NodeIndex srcNode, PtsGraph &ptsGraph)
    {
        //OP << "NodeFactory:\n";
        //OP<<*I<<"\n";
        //OP<<"offset = "<<offset<<"\n";
        //OP<<"Inside handleContainerOf:\n";
        //OP<<"srcNode = "<<srcNode<<"\n";
        assert(I->getType()->isPointerTy() && "passing non-pointer type to handleContainerOf");
        assert(cast<PointerType>(I->getType())->getElementType()->isIntegerTy(8) && "handleContainerOf can only handle i8*");
        assert(offset < 0 && "handleContainerOf can only handle negative offset");
        NodeIndex gepIndex = nodeFactory.getValueNodeFor(I);
        // for each obj the pointer can point-to
        // hopefully there'll only be one obj <= too young too naive, function: drm_vma_offset_lookup_locked, store nullptr or arg to it.
        //assert(ptsGraph[srcNode].getSize() == 1);
        //OP<<"srcNode = "<<srcNode<<"\n";
        for (NodeIndex obj : ptsGraph[srcNode])
        {
            //OP<<"obj for src = "<<obj<<"\n";
            // if src obj special, ptr arithmetic is meaningless
            if (obj <= nodeFactory.getConstantIntNode())
                return 0;

            // find the allocated type
            const Value *allocObj = nodeFactory.getValueForNode(obj);
            const Type *allocType = allocObj->getType();

            // 1. check if allocated type is okay
            assert(allocType->isPointerTy());
            const StructType *stType = dyn_cast<StructType>(cast<PointerType>(allocType)->getElementType());
            if (!stType)
            {
                OP << "!stType, the precise cannot be promised.\n";
                return 0;
            }
            //assert(stType && "handleContainerOf can only handle struct type");
            //OP<<"stType: "<<*stType<<"\n";
            const StructInfo *stInfo = Ctx->structAnalyzer.getStructInfo(stType, M);
            assert(stInfo != NULL && "structInfoMap should have info for st!");
            // find the size offset of src ptr
            // int64_t should be large enough for unsigned
            //srcField: the obj field from the current obj
            int64_t srcField = nodeFactory.getObjectOffset(obj);
            //OP<<"srcField = "<<srcField<<"\n";
            //OP<<"offset vec size = "<<stInfo->getFieldSize()<<"\n";
            //szieOffset: the offset from the current obj
            if (srcField >= stInfo->getExpandedSize())
                continue;
            int64_t sizeOffset = stInfo->getFieldOffset(srcField);
            //OP<<"sizeOffset = "<<sizeOffset<<"\n";
            // check if the size offset of src ptr is large enough to handle the negative offset
            //OP<<"sizeOffset + offset = "<<sizeOffset + offset<<"\n";
            if (sizeOffset + offset >= 0)
            {
                // FIXME: assume only one obj
                int64_t dstField = offsetToFieldNum(I->getOperand(0)->stripPointerCasts(), sizeOffset + offset,
                                                    DL, &Ctx->structAnalyzer, M);
                //OP<<"dstField = "<<dstField<<"\n";
                //? src - dst?
                return (int)(dstField - srcField);
            }

            // 2. If the allocated type is not large enough to handle the offset,
            //    create a new one and update the ptr2set
            // get real type from later bitcast
            const Type *realType = nullptr;
            const Value *castInst = nullptr;
            for (auto U : I->users())
            {
                //OP<<"U: "<<*U<<"\n";
                if (isa<BitCastInst>(U))
                {
                    realType = U->getType();
                    castInst = U;
                    //break;
                    //#ifdef nonsense
                    //comment for function queue_attr_show in blk-sysfs, var %24.
                    if (isa<GetElementPtrInst>(U->getOperand(0)))
                    {
                        ptsGraph[gepIndex] = ptsGraph[srcNode];
                        OP << "gep inst, the precise cannot be promised.\n";
                        return 0;
                    }
                    //#endif
                }
            }
            //assertion disabled: function dm_per_bio_data
            if (!realType)
            {
                OP << "Failed to find the dst type for container_of\n";
                return 0;
            }
            //assert(realType && "Failed to find the dst type for container_of");
            assert(realType->isPointerTy());
            //special case: function dequeue_func
            const StructType *rstType = dyn_cast<StructType>(cast<PointerType>(realType)->getElementType());

            if (!rstType)
            {
                OP << "handleContainerOf can only handle struct type\n";
                return 0;
            }
            OP<<"2\n";
            //assert(rstType && "handleContainerOf can only handle struct type");
            //
            // this is the tricky part, we need to find the field number inside the larger struct
            const StructInfo *rstInfo = Ctx->structAnalyzer.getStructInfo(rstType, M);
            assert(rstInfo != nullptr && "structInfoMap should have info for rst!");

            //double check the container type?
            // FIXME: assuming one container_of at a time
            // fix stInfo if srcField is not 0
            bool found_container = false;
            std::set<Type *> elementTypes = stInfo->getElementType(srcField);
            //OP<<"elementTypes size:"<<elementTypes.size()<<"\n";
            for (Type *t : elementTypes)
            {
                //OP<<"elementType: "<<*t<<"\n";
                if (StructType *est = dyn_cast<StructType>(t))
                {
                    OP<<"est type: "<<*est<<"\n";

                    const StructInfo *estInfo = Ctx->structAnalyzer.getStructInfo(est, M);
                    assert(estInfo != nullptr && "structInfoMap should have info for est!");
                    if (estInfo->getContainer(rstInfo->getRealType(), -offset))
                    {
                        found_container = true;
                        break;
                    }
                }
            }
            if (!found_container)
            {
                OP << "found_container fails, the precise cannot be promised.";
                return 0;
            }
            //assert(found_container && "Failed to find the container");
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
            //OP<<"valNode: "<<valNode<<"\n";
            ptsGraph[gepIndex].insert(castObjNode);
            // next, update the ptr2set to the new object
            unsigned subField = offsetToFieldNum(castInst, -offset, DL, &Ctx->structAnalyzer, M);
            //OP<<"subField = "<<subField<<", castObjNode = "<<castObjNode<<"\n";
            NodeIndex newObjNode = nodeFactory.getOffsetObjectNode(castObjNode, subField);
            //OP<<"newObjNode = "<<newObjNode<<"\n";
            updateObjectNode(obj, newObjNode, ptsGraph);

            if (nodeFactory.isArgNode(obj))
            {
                unsigned newSize = nodeFactory.getObjectSize(newObjNode);
                unsigned newOffset = nodeFactory.getObjectOffset(newObjNode);
                for (unsigned i = 0; i < newSize; i++)
                {
                    nodeFactory.setArgNode(newObjNode - newOffset + i);
                }
            }
            // update the value -> obj map
            //OP<<"newObjNode = "<<newObjNode<<"\n";
            //OP<<"update\n";
            nodeFactory.updateNodeForObject(allocObj, newObjNode);
            // finally, return the offset/fieldNum
            //OP<<"-subField =  "<<-subField<<"\n";
            return -subField;
        } // end for all obj in ptr2set
        return 0;
    }
#ifdef instFieldNUM
    int64_t getGEPInstFieldNum(const GetElementPtrInst *gepInst, const DataLayout *dataLayout, const StructAnalyzer &structAnalyzer, Module *module)
    {
        int64_t offset = getGEPOffset(gepInst, dataLayout);
        //OP<<"offset = "<<offset<<"\n";
        if (offset < 0)
            return offset;

        const Value *ptr = gepInst->getPointerOperand()->stripPointerCasts();
        Type *trueElemType = cast<PointerType>(ptr->getType())->getElementType();

        unsigned ret = 0;
        while (offset > 0)
        {
            // Collapse array type
            while (const ArrayType *arrayType = dyn_cast<ArrayType>(trueElemType))
                trueElemType = arrayType->getElementType();

            //errs() << "trueElemType = "; trueElemType->dump(); errs() << "\n";
            if (trueElemType->isStructTy())
            {
                StructType *stType = cast<StructType>(trueElemType);
                //OP<<*stType<<"\n";
                const StructInfo *stInfo = structAnalyzer.getStructInfo(stType, module);
                //OP<<"stInfo:\n";
                //structAnalyzer.printStructInfo();
                assert(stInfo != NULL && "structInfoMap should have info for all structs!");

                stType = const_cast<StructType *>(stInfo->getRealType());
                const StructLayout *stLayout = stInfo->getDataLayout()->getStructLayout(stType);

                offset %= stInfo->getDataLayout()->getTypeAllocSize(stType);
                unsigned idx = stLayout->getElementContainingOffset(offset);
                ret += stInfo->getOffset(idx);
                offset -= stLayout->getElementOffset(idx);
                trueElemType = stType->getElementType(idx);
            }
            else
            {
                if (offset %= dataLayout->getTypeAllocSize(trueElemType) != 0)
                {
                    errs() << "Warning: offset " << offset << " into the middle of a field. This usually occurs when union is used. Since partial alias is not supported, correctness is not guanranteed here.\n";
                    break;
                }
            }
        }
        return ret;
    }
#endif
    void QualifierAnalysis::calSumForRec(std::vector<llvm::Function *> & rec)
    {
	unsigned temp = 1;
        OP << "calculate summary for " << rec.size() << " recursive functions: \n";
        std::queue<llvm::Function *> scQueue;
        std::unordered_map<llvm::Function*, bool> inQueue;
        Ctx->ChangeMap.clear();
        for (auto item : rec)
        {
            scQueue.push(item);
            inQueue[item] = true;
            Ctx->ChangeMap[item] = true;
        }
        bool change = false;
        while (!scQueue.empty())
        {
            change = false;
            llvm::Function *func = scQueue.front();
            scQueue.pop();
            inQueue[func] = false;

            Summary in;
        #ifdef LOADSUM_
            if (Ctx->FSummaries.find(func) == Ctx->FSummaries.end())
            {
                /* Name of function summary: relative path + module name + function name + .sum*/
                std::string fname = getScopeName(func, Ctx->funcToMd, Ctx->Funcs);
                std::string fsName = fname + ".sum";

                OP << "fname = " << fname << "\n";
                std::string sFile = oldSDir + fsName + ".sum";
                OP << "sFile = " << sFile << "\n";

                std::ifstream ifile(sFile);
                if (ifile)
                {
                    OP << "file exists!\n";
                    //std::ifstream ifile(sFile);
                    boost::archive::text_iarchive ia(ifile);
                    ia >> Ctx->FSummaries[func];
                }
            }
        #endif
            if (Ctx->FSummaries.find(func) != Ctx->FSummaries.end())
            {
                in.copySummary(in, Ctx->FSummaries[func], func);
            }
	    
            Ctx->impact[func] = calCallee(func, Ctx->CallMaps); 
#ifdef RUN_ON_FUNC
            runOnFunction(func, false);
#endif
            if (!Ctx->FSummaries[func].equal(in))
            {
                scQueue.push(func);
                inQueue[func] = true;
                Ctx->ChangeMap[func] = true;
            }
            else {
                Ctx->ChangeMap[func] = false;
                for (auto item : rec) {
                    change |= Ctx->ChangeMap[item];
                    if (change) break;
                }
                if (!change) {
                    break;
                }
            }
        } //while (!scQueue.empty())

        printWarnForScc(rec);
        for (auto item : rec) {
            Ctx->Visit[item] = true;
        }
        //OP << readyCount << " rec functions are done.\n";
    }

    static bool isCastingCompatibleType(Type * T1, Type * T2)
    {
        if (T1->isPointerTy())
        {
            if (!T2->isPointerTy())
                return false;

            Type *ElT1 = T1->getPointerElementType();
            Type *ElT2 = T2->getPointerElementType();
            // assume "void *" and "char *" are equivalent to any pointer type
            if (ElT1->isIntegerTy(8) /*|| ElT2->isIntegerTy(8)*/)
                return true;
            //if(ElT1->isStructTy())
            //return true;

            return isCastingCompatibleType(ElT1, ElT2);
        }
        else if (T1->isArrayTy())
        {
            if (!T2->isArrayTy())
                return false;
            Type *ElT1 = T1->getArrayElementType();
            Type *ElT2 = T2->getArrayElementType();
            return isCastingCompatibleType(ElT1, ElT1);
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
            return true;
        }
        else
        {
            errs() << "Unhandled Types:" << *T1 << " :: " << *T2 << "\n";
            return T1->getTypeID() == T2->getTypeID();
        }
    }
void FuncAnalysis::addTerminationBB(llvm::BasicBlock* bb) {
    OP<<"addTerm "<<bb->getName().str()<<"\n";
    int count = 0;
    llvm::BasicBlock *cur = bb;
    std::unordered_set<llvm::BasicBlock*> visit;
    visit.insert(cur);
    for (auto si = succ_begin(cur), se = succ_end(cur); si != se; ++si) count++;
    while(count == 1) {
	count = 0;
	//OP<<"cur ="<<cur->getName().str()<<"count = "<<count<<"\n";
	for (auto si = succ_begin(cur), se = succ_end(cur); si != se; ++si) {
 	    if (visit.count(*si)) continue;
	    llvm::BasicBlock* temp  = *si; 
	    visit.insert(cur);
	    count++;
	    //OP<<"cur->size = "<<cur->size()<<"\n";
	    if (temp->size() > 1) {
		count = 0;
		break;
	    }
	    cur = temp;
	}
    }	
    terminationBB.insert(cur);
    OP<<"insert "<<cur->getName().str()<<"\n";
}
