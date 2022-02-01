#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/FunctionComparator.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/DebugInfo.h"
#include <llvm/IR/InstIterator.h>
#include <memory>
#include <vector>
#include <sstream>
#include <sys/resource.h>
#include <fstream>
#include <time.h>
#include <cstring>
#include <typeinfo>
#include <utility>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <unordered_set>
#include "llvm/Bitcode/BitcodeReader.h"

#include "UBIAnalysis.h"
#include "CallGraph.h"
#include "Config.h"
#include "QualifierAnalysis.h"
#include "json11.hpp"

using namespace llvm;
#define LOAD_NEWFILE
//#define PRINT_CHANGE_FILE
//#define FILE_LOADING_DBG
//#define TEST
//#define CG_DEBUG
//#define CANDICG_DEBUG
#define REST_F
//#define _PRINT_STMAP
#define CAL_REC
std::string diffMd="";
std::string diffFunctions = "";
//#define _PRINT_MODIFIEDFUNCS
//#define LOADING_DBG
//#define CG_JSON
// Command line parameters.
cl::list<std::string> InputFilenames(
		cl::Positional, cl::OneOrMore, cl::desc("<input bitcode files>"));

//cl::opt<unsigned> VerboseLevel(
//    "verbose-level", cl::desc("Print information at which verbose level"),
//    cl::init(0));

cl::opt<bool> UniAnalysis(
		"ubi-analysis",
		cl::desc("Analysis uninitialized pointer dereference"),
		cl::NotHidden, cl::init(false));
cl::opt<bool> UbiIncAnalysis(
		"inc",
		cl::desc("Doing incremental analysis"),
		cl::NotHidden, cl::init(false));

GlobalContext GlobalCtx;

void IterativeModulePass::run(ModuleList &modules)
{
	ModuleList::iterator i, e;
	OP << "[" << ID << "] Initializing " << modules.size() << " modules ";
	bool again = true;

	while (again)
	{
		again = false;
		for (i = modules.begin(), e = modules.end(); i != e; ++i)
		{
			again |= doInitialization(i->first);
			OP << ".";
		}
	}
	OP << "\n";
#ifdef _FIND_DEF
	OP << "print the name -> funcDef map: \n";
	OP << "size of Ctx->nameFuncs = " << Ctx->nameFuncs.size() << "\n";
	for (auto item : Ctx->nameFuncs)
	{
		if (item.second.size() > 1)
		{
			OP << "name: " << item.first << ": \n";
			for (auto addr : item.second)
			{
				OP << addr << " in module " << addr->getParent()->getName().str() << "\n";
			}
		}
	}

#endif
	//calculate functions not enrolled in loops

	unsigned iter = 0, changed = 1;
	while (changed)
	{
		++iter;
		changed = 0;
		for (i = modules.begin(), e = modules.end(); i != e; ++i)
		{
			OP << "[" << ID << " / " << iter << "] ";
			OP << "[" << i->second << "]\n";

			bool ret = doModulePass(i->first);
			if (ret)
			{
				++changed;
				OP << "\t [CHANGED]\n";
			}
			else
				OP << "\n";
		}
		OP << "[" << ID << "] Updated in " << changed << " modules.\n";
	}

	//calculate the scc iteratively
#ifdef CAL_REC
	/* Collect functions that enrolled in recursive calls. */
	collectRemaining();
	/*do module pass again and deal with callers of loop enrolled functions*/
	changed = 1;
	while (changed)
	{
		++iter;
		changed = 0;
		for (i = modules.begin(), e = modules.end(); i != e; ++i)
		{
			OP << "[" << ID << " / " << iter << "] ";
			OP << "[" << i->second << "]\n";

			bool ret = doModulePass(i->first);
			if (ret)
			{
				++changed;
				OP << "\t [CHANGED]\n";
			}
			else
				OP << "\n";
		}
		OP << "[" << ID << "] Updated in " << changed << " modules.\n";
	}
#endif
	//Finalization
	OP << "[" << ID << "] Postprocessing ...\n";
	again = true;
	while (again)
	{
		again = false;
		for (i = modules.begin(), e = modules.end(); i != e; ++i)
		{
			// TODO: Dump the results.
			again |= doFinalization(i->first);
		}
	}
	OP << "Finalizing ...\n";
	finalize();
	OP << "[" << ID << "] Done!\n\n";
}

void LoadStaticData(GlobalContext *GCtx)
{
	// load functions for heap allocations
	SetHeapAllocFuncs(GCtx->HeapAllocFuncs);
	SetInitFuncs(GCtx->InitFuncs);
	SetCopyFuncs(GCtx->CopyFuncs);
	SetTransferFuncs(GCtx->TransferFuncs);
	SetObjSizeFuncs(GCtx->ObjSizeFuncs);
	SetZeroMallocFuncs(GCtx->ZeroMallocFuncs);
	SetIndirectFuncs(GCtx->IndirectFuncs);
	SetStrFuncs(GCtx->StrFuncs);
	SetPreSumFuncs(GCtx->PreSumFuncs);
	SetSyscallFuncs(GCtx->SyscallFuncs);
	//SetEntryFuncs(GCtx->EntryFuncs);
	//SetManualSummarizedFuncs(GCtx->ManualSummaries);
}
inline size_t fHash(std::string &fName, std::string &relMName)
{
	std::hash<std::string> str_hash;
	std::string output = relMName + fName;

	return str_hash(output);
}
StringRef getOldName(std::string str, std::string &oldDir, std::string &newDir)
{
	str.erase(0, newDir.size());
	str = oldDir + str;
	return StringRef(str);
}
void findPath(CalledMap &C, llvm::Function *src) {
	std::string  dst = "printk";
	std::unordered_map<llvm::Function*, bool> inQueue;
	std::queue<llvm::Function*> q{{src}};
	inQueue[src] = true;
	int layer = 0;
	while (!q.empty()) {
		int sz = q.size();
		for (int k = 0; k < sz; k++) {
			layer++;
			llvm::Function *cur = q.front();q.pop();
			if (cur->getName().str() == dst) break;
			for (auto caller : C[cur]) {
				if (!inQueue.count(caller)) {
					q.push(caller);
					inQueue[caller] = true;
				}
			}
		}	    
	}
	OP<<"layer = "<<layer<<"\n";
}
void printCGJson(ModuleList &Modules)
{
	std::ofstream cgFile;
	cgFile.open("/home/ubuntu/experiment/test/CG/hybridCG.json", std::ios::app);
	for (auto ml : Modules)
	{
		for (Function &f : *ml.first)
		{
			Function *F = &f;
			std::string caller = F->getName().str();
			std::string directCall = "";
			for (BasicBlock &BB : *F)
			{
				llvm::BasicBlock *b = &BB;
				std::string bb = b->getName().str();
				for (Instruction &i : *b)
				{
					llvm::Instruction *Ins = &i;
					if (llvm::CallInst *CI = dyn_cast<CallInst>(Ins))
					{
						if (CI->getCalledFunction()) {
							std::string funcName = CI->getCalledFunction()->getName().str();
							directCall = directCall + funcName + "$$"+CI->getCalledFunction()->getParent()->getName().str()+"##";
						}

					}
				}
			}
			json11::Json::object jsonObj = json11::Json::object{
				{"function", caller+"$$"+ml.first->getName().str()},
					{"direct:", directCall.substr(0, directCall.size() - 2)}
			};

			for (BasicBlock &BB : *F) {
				llvm::BasicBlock *b = &BB;
				for (Instruction &i : *b) {
					llvm::Instruction *Ins = &i;
					if (llvm::CallInst *CI = dyn_cast<CallInst>(Ins)) {
						if (!CI->getCalledFunction()) {
							if (InlineAsm *ASM = dyn_cast<InlineAsm>(CI->getCalledValue()))
								continue;


							std::string insStr;
							llvm::raw_string_ostream rso(insStr);
							Ins->print(rso);
							std::string valueStr = "";
							for (auto ff : GlobalCtx.Callees[CI])
							{   
								llvm::Function *callee = &*ff;
								std::string fname = callee->getName().str();
								valueStr = valueStr + fname + "$$" + callee->getParent()->getName().str() + "##";
							}
							jsonObj[insStr] = valueStr.substr(0, valueStr.size() - 2);
						}
					}
				}
			}
			json11::Json json_wrapper = json11::Json{jsonObj};
			std::string json_str = json_wrapper.dump();
			cgFile << json_str << "\n";
		}
	}
}
void InitReadyFunction(GlobalContext *Ctx)
{
	ModuleList moduleList = Ctx->Modules;
	if (Ctx->incAnalysis)
	{
		moduleList = Ctx->candiModules;
	}

	for (auto ml : moduleList)
	{
		for (Function &f : *ml.first)
		{
			Function *F = &f;
			if (!F->empty())
			{
				if (!Ctx->incAnalysis)
				{
					Ctx->Visit[F] = false;
					if (Ctx->CallMaps.find(F) == Ctx->CallMaps.end())
					{
						Ctx->ReadyList.insert(F);
						Ctx->tasks.push(F);
					}
					else
					{
						if (Ctx->CallMaps[F].size() == 0)
						{
							Ctx->ReadyList.insert(F);
							Ctx->tasks.push(F);
						}
					}
					if (Ctx->CallMaps.find(F) == Ctx->CallMaps.end() && 
							Ctx->CalledMaps.find(F) == Ctx->CalledMaps.end()) {
						Ctx->indFuncs.insert(F);
					}
				}
				else
				{
					if (Ctx->incCandiFuncs.count(F))
					{
						Ctx->Visit[F] = false;
						if (Ctx->candiCallMaps.find(F) == Ctx->candiCallMaps.end() && 
								Ctx->candiCalledMaps.find(F) == Ctx->candiCalledMaps.end()) {
							Ctx->indFuncs.insert(F);
						}
						if (Ctx->candiCallMaps.find(F) == Ctx->candiCallMaps.end() || (Ctx->candiCallMaps[F].size() == 0))
						{
							Ctx->ReadyList.insert(F);
						}
					}
				}
			}
		}
	}
}

std::vector<std::string> split(const std::string& str, const std::string& delim) {
	std::vector<std::string> res;
	if("" == str) return res;
	char * strs = new char[str.length() + 1] ; 
	strcpy(strs, str.c_str()); 

	char * d = new char[delim.length() + 1];
	strcpy(d, delim.c_str());

	char *p = strtok(strs, d);
	while(p) {
		std::string s = p;
		res.push_back(s);
		p = strtok(NULL, d);
	}

	return res;
}

/*Iterate though the functions that changes, if the function is leaf node, then the remained function is 0:
  meaning it's ready to analyze; otherwise it could be affected by lower callee. We hold it here and do a BFS from
  changed leaf node.  Finally if the remained function of the modified functions are not calculated, meaning it does not
  be affected by the lower calee, then we initialize its remained function as 0.
 * Ex: C1->C2
 *     C3->F->...
 * 1. C1/C2: set the remained function as 0
 * 2. Propagate from C2 and increase C1 by 1
 */
void setUpOutput(GlobalContext *Ctx, std::string oldDir, std::string newDir) {
	Ctx->absPath=newDir;
	Ctx->outFolder=oldDir+"/ubi-ia-out/";
	Ctx->incFolder=newDir+"/ubi-ia-out/";

	Ctx->jsonFile = Ctx->outFolder+"/ubiWarn.json";
	Ctx->ioJsonFile = Ctx->outFolder+"/ioWarn.json";
	Ctx->oobJsonFile = Ctx->outFolder+"/oobWarn.json";

	Ctx->jsonInc = Ctx->incFolder+"/ubiIncWarn.json";
	Ctx->ioJsonIncFile = Ctx->incFolder+"/ioIncWarn.json";
	Ctx->oobJsonIncFile = Ctx->incFolder+"/oobIncWarn.json";	

	std::vector<std::string> newDirVec=split(newDir, "/");
	std::vector<std::string> oldDirVec=split(oldDir, "/");
	Ctx->newVersion=newDirVec.at(newDirVec.size()-1).substr(4);
	Ctx->oldVersion=oldDirVec.at(oldDirVec.size()-1).substr(4);

	diffMd="/home/IncreLux/example/"+Ctx->newVersion+"-"+Ctx->oldVersion+"/md.txt";
	diffFunctions = "/home/IncreLux/example/"+Ctx->newVersion+"-"+Ctx->oldVersion+"/bc+func.json";	
	Ctx->newVersion+="_";
	Ctx->oldVersion+="_";
	errs()<< Ctx->absPath<<"\n";
	errs()<<Ctx->outFolder<<"\n";
	errs()<<Ctx->incFolder<<"\n";
	errs()<<Ctx->jsonFile<<"\n";
	errs()<<Ctx->oobJsonIncFile<<"\n";
	errs()<<"Ctx->newVersion = "<<Ctx->newVersion<<"\n";
	errs()<<"Ctx->oldVersion = "<<Ctx->oldVersion<<"\n";
	errs()<<"diffMd="<<diffMd<<"\n";
	errs()<<"diffFunctions="<<diffFunctions<<"\n";
}
int setUpIncFunctions(GlobalContext *Ctx)
{
	OP << "Inside setUpIncFunctions:\n";
	/* diffFunctions: module name + function*/
	OP<<"diffFunctions = "<<diffFunctions<<"\n";
	std::ifstream ifile(diffFunctions);
	std::string funcName;
	std::set<std::string> funcNames;
	std::string jline;
	/*For BFS traverse.*/
	std::queue<llvm::Function *> candiFuncs;
	llvm::Function *src = NULL;
	int count1 = 0, count2 = 0;
	/* 1. Get the changed functions from the prepared files. */
	getline(ifile, jline);
	while (!ifile.eof())
	{
		OP<<"jline : "<<jline<<"\n";
		std::string err;
		const auto jstr = json11::Json::parse(jline, err);
		std::string moduleName = jstr["md"].string_value();
		std::string funcName = jstr["function"].string_value();

		if (funcName == "printk" || funcName =="kfree"|| funcName =="dev_err"){
			getline(ifile, jline);
			continue;
		}
		NameFuncMap::iterator it = Ctx->Funcs.find(funcName);
		/* A global visible function*/
		if (it != Ctx->Funcs.end())
		{
			llvm::Function *F = it->second;

			if (Ctx->modifiedFuncs.find(F) == Ctx->modifiedFuncs.end())
			{
				Ctx->modifiedFuncs.insert(F);
				Ctx->incCandiFuncs[F] = true;
				candiFuncs.push(F);
				OP<<"candiFuncs.push("<<F->getName().str()<<")\n";
			}
		}
		/* A local visible function*/
		else
		{
			OP<<"cal hash for "<<funcName<<", "<<moduleName<<"\n";
			size_t fh = fHash(funcName, moduleName);
			if (Ctx->localFuncs.find(fh) != Ctx->localFuncs.end())
			{
				OP << "==>moduleName2 = " << moduleName << ", funcName = " << funcName << "\n";
				llvm::Function *F = Ctx->localFuncs[fh];
				Ctx->modifiedFuncs.insert(F);
				Ctx->incCandiFuncs[F] = true;
				candiFuncs.push(F);
				OP<<"2candiFuncs.push("<<F->getName().str()<<")\n";
			}
			else
			{
				OP << "Not found\n";
			}
		}
		getline(ifile, jline);
	}
	/* 1-2: Get function from new files*/
	for (auto *m : Ctx->newCompiledFiles) {
		llvm::Module *Md = m;
		//OP<<"New compiled file :"<<Md->getName().str()<<"\n";
		for (Module::iterator iter = Md->begin(); iter != Md->end(); iter++) {
			llvm::Function *func = &*iter;
			if (func->empty()) {
				//auto FIter = Ctx->Funcs.find(func->getName().str());
				//if (FIter != Ctx->Funcs.end()){
				//    func = FIter->second;
				// }
				continue;
			}
			if (Ctx->modifiedFuncs.find(func) == Ctx->modifiedFuncs.end()) {
				Ctx->modifiedFuncs.insert(func);
				Ctx->incCandiFuncs[func] = true;
				candiFuncs.push(func);
			}
		}
	}

	/* 2. Using BFS to traverse from the leaf to the root, find all the candi functions. */
	std::unordered_set<llvm::Function *> visited;
	while (!candiFuncs.empty())
	{
		llvm::Function *cur = candiFuncs.front();
		candiFuncs.pop();
		for (auto caller : Ctx->CalledMaps[cur])
		{
			if (caller->getName().str() == "printk" || caller->getName().str() =="kfree"|| caller->getName().str() =="dev_err")
				continue;

			if (Ctx->incCandiFuncs.count(caller))
			{
				Ctx->incCandiFuncs[caller] |= false;
			}
			else
			{
				Ctx->incCandiFuncs[caller] = false;
			}

			if (!visited.count(caller))
			{
				//OP<<"1candiFuncs.push("<<caller->getName().str()<<")\n";
				candiFuncs.push(caller);
			}
		}
		visited.insert(cur);
	}
	int count = 0, maxx = 0;
	llvm::Function *cc = NULL;
	std::map<llvm::Function *, int> calleeTimes;
	/*3. Now we got all the candiFuncs, and Constructing the candiCG. */
	for (auto item : Ctx->CalledMaps) {
		llvm::Function *callee = item.first;
		//if (callee->getName().str() == "printk" || callee->getName().str() =="kfree"|| callee->getName().str() =="dev_err")
		//	continue;
		if (!Ctx->incCandiFuncs.count(callee))
		{
			continue;
		}
		count = 0;
		for (auto caller : Ctx->CalledMaps[callee])
		{
			if (Ctx->incCandiFuncs.count(caller))
			{
				count++;
				Ctx->candiCallMaps[caller].insert(callee);
				Ctx->candiCalledMaps[callee].insert(caller);
			}
		}
		calleeTimes[callee] = count;
		if (count > maxx) {
			maxx = count;
			cc = callee;
		}
	}
	typedef std::function<bool(std::pair<llvm::Function*, int>, std::pair<llvm::Function*, int>)> Comparator;
	// Defining a lambda function to compare two pairs. It will compare two pairs using second field
	Comparator compFunctor =
		[](std::pair<llvm::Function*, int> elem1 ,std::pair<llvm::Function*, int> elem2)
		{
			return elem1.second > elem2.second;
		};

	// Declaring a set that will store the pairs using above comparision logic
	std::set<std::pair<llvm::Function*, int>, Comparator> setOfTimes(
			calleeTimes.begin(), calleeTimes.end(), compFunctor);

	// Iterate over a set using range base for loop
	// It will display the items in sorted order of values
	for (std::pair<llvm::Function*, int> element : setOfTimes)
		OP << element.first->getName().str() << " :: " << element.second << "\n";



	//src = *Ctx->modifiedFuncs.begin();
	//OP<<"src: "<<src->getName().str()<<"\n";
	//findPath(Ctx->candiCalledMaps, src);
	if (maxx > 0)
		OP<<"maxx = "<<maxx<<",Func: "<<cc->getName().str()<<"\n";


	/* 4. Using candiCallMaps to calculate the incFuncRemained. */
	for (auto i : Ctx->candiCallMaps)
	{
		Ctx->incFuncRemained[i.first] = i.second.size();
	}

	/* 5. set up the candi modules. */
	std::unordered_set<llvm::Module *> inserted;
	for (auto item : Ctx->incCandiFuncs)
	{
		llvm::Module *md = item.first->getParent();
		if (inserted.count(md))
			continue;
		inserted.insert(md);
		Ctx->candiModules.push_back({md, Ctx->funcToMd[item.first]});
	}
	OP << "candiModules.size() = " << Ctx->candiModules.size() << "\n";
	OP << "incCandiFuncs.size() = " << Ctx->incCandiFuncs.size() << ", Ctx->incFuncRemained.size() = " << Ctx->incFuncRemained.size() << "\n";
#ifdef DBG
	OP << "candiFuncs: \n";
	for (auto item : Ctx->incCandiFuncs)
	{
		OP << "--" << item.first->getName().str() << "\n";
	}
#endif
	return Ctx->modifiedFuncs.size();
}

int main(int argc, char **argv)
{
	clock_t totalsTime, totaleTime;
	clock_t sTime, eTime;
	OP << "argc = " << argc << "\n";
	/* Print a stack trace if we signal out. */
	sys::PrintStackTraceOnErrorSignal(argv[0]);
	PrettyStackTraceProgram X(argc, argv);

	llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

	cl::ParseCommandLineOptions(argc, argv, "global analysis\n");
	SMDiagnostic Err;
	totalsTime = clock();
	/* Loading modules for incremental analysis */
	if (UbiIncAnalysis)
	{
		OP << "Total " << InputFilenames.size() - 2 << " file(s)\n";
		std::string oldDir = argv[argc - 2];
		std::string newDir = argv[argc - 1];
		GlobalCtx.incAnalysis = true;
		setUpOutput(&GlobalCtx, oldDir, newDir);

		std::ifstream ifile(diffMd);
		std::string mdName;
		std::unordered_map<std::string, std::string> changeFrom;
		/*Calculate the changed file, changeFrom: newer version(4.15) from old one (4.14)*/
		while (!ifile.eof())
		{
			getline(ifile, mdName);
			if (mdName.size() == 0)
				continue;
			std::string oldMdName = oldDir + mdName;
			std::string newMdName = newDir + mdName;
			/*newMdName changes from oldMdName*/
			changeFrom[newMdName] = oldMdName;
		}

#ifdef PRINT_CHANGE_FILE
		OP << "size of changeFrom " << changeFrom.size() << "\n";
		for (auto item : changeFrom)
		{
			OP << "Changed file " << item.first << "\n";
			OP << "From " << item.second << "\n";
		}
#endif

		/*Load the files to be analyzed and construct Ctx->Modules*/
		for (unsigned i = 0; i < InputFilenames.size() - 2; ++i)
		{
			LLVMContext *LLVMCtx = new LLVMContext();
			StringRef MName = StringRef(strdup(InputFilenames[i].data()));
			std::unique_ptr<Module> M = NULL;
			int fd;
			struct stat sb;
			char *mapped;

			bool newCompiledFiles = false;
			if (changeFrom.find(MName.str()) == changeFrom.end()) { 
				std::string temp=InputFilenames[i];	
				std::string::size_type pos(0);
				if((pos=temp.find(newDir))!=std::string::npos)
					temp.replace(pos,newDir.length(),oldDir);
				if ((fd = open(temp.c_str(), O_RDWR)) < 0){
					newCompiledFiles = true;
				}
				else{
					close(fd);	
				}
			}
			/* case 1: Modules that change */
			if (changeFrom.find(MName.str()) != changeFrom.end() || newCompiledFiles)
			{
				M = parseIRFile(MName, Err, *LLVMCtx);
				if (M == NULL)
				{
					OP << argv[0] << ": error loading file 1'"
						<< MName.str() << "'\n";
					continue;
				}
				OP<<"Loading new file :"<<InputFilenames[i].c_str()<<"\n";
				/*map the new file*/
				if ((fd = open(InputFilenames[i].c_str(), O_RDWR)) < 0){
					perror("open");
				}

				if ((fstat(fd, &sb)) == -1)
				{
					perror("fstat");
				}

				if ((mapped = (char *)mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void *)-1)
				{
					perror("mmap");
					OP << InputFilenames[i].c_str() << " not mapped.\n";
				}
				close(fd);
				Module *temp = M.release();
				GlobalCtx.Modules.push_back(std::make_pair(temp, temp->getName()));
				GlobalCtx.ModuleMaps[temp] = temp->getName();
				if (newCompiledFiles) {
					GlobalCtx.newCompiledFiles.push_back(temp);
				}
			}
			/*case 2: the modules that keeps the same*/
			else
			{
				std::string::size_type   pos(0);
				if((pos=InputFilenames[i].find(newDir))!=std::string::npos)
					InputFilenames[i].replace(pos,newDir.length(),oldDir);
				MName = StringRef(strdup(InputFilenames[i].data()));
				bool loaded = false;
				if ((fd = open(MName.str().c_str(), O_RDWR)) < 0)
				{
					perror(MName.str().c_str());
				}
				if ((fstat(fd, &sb)) == -1)
				{
					OP << "stat:\n";
					perror(MName.str().c_str());
					continue;
				}
				else
				{
					if (sb.st_mode & S_IFDIR)
					{
						continue;
					}
				}

				if ((mapped = (char *)mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void *)-1)
				{
					perror("mmap");
					OP << MName.str() << " not mapped.\n";
				}
				close(fd);

				std::string IRString(mapped, mapped + sb.st_size);
				std::unique_ptr<MemoryBuffer> memBuffer = MemoryBuffer::getMemBuffer(IRString);
				MemoryBuffer *mem = memBuffer.release();
				//Expected<std::unique_ptr<Module> > ModuleOrErr = parseBitcodeFile(mem->getMemBufferRef(), *LLVMCtx);
				M = parseIR(mem->getMemBufferRef(), Err, *LLVMCtx);

				if (M == NULL)
				{
					OP << argv[0] << ": error loading file '"
						<< InputFilenames[i] << "'\n";
					continue;
				}
				Module *Md = M.release();
				GlobalCtx.Modules.push_back(std::make_pair(Md, MName));
				GlobalCtx.ModuleMaps[Md] = MName;
			}

		}
	}
	/* Load files for non-inc analysis */
	if (UniAnalysis)
	{
		OP << "Total " << InputFilenames.size() -1 << " file(s)\n";
		GlobalCtx.absPath = argv[argc - 1];
		GlobalCtx.outFolder = GlobalCtx.absPath+"/ubi-ia-out/";
		GlobalCtx.jsonFile = GlobalCtx.outFolder+"/ubiWarn.json";
		for (unsigned i = 0; i < InputFilenames.size(); ++i)
		{
			LLVMContext *LLVMCtx = new LLVMContext();
			std::unique_ptr<Module> M = parseIRFile(InputFilenames[i], Err, *LLVMCtx);

			if (M == NULL)
			{
				OP << argv[0] << ": error loading file '"
					<< InputFilenames[i] << "'\n";
				OP<<Err.getMessage();
				continue;
			}

			Module *Module = M.release();
			StringRef MName = StringRef(strdup(InputFilenames[i].data()));
			GlobalCtx.Modules.push_back(std::make_pair(Module, MName));
			GlobalCtx.ModuleMaps[Module] = InputFilenames[i];
		}
	}

	/* Load hard coded functions */
	LoadStaticData(&GlobalCtx);

	clock_t cg_sTime, cg_eTime;
	cg_sTime=clock(); 
	/* Construct the call graph */
	CallGraphPass CGPass(&GlobalCtx);
	CGPass.run(GlobalCtx.Modules);
	cg_eTime=clock();
	errs()<<"CG_Time =  "<<(double)(cg_eTime-cg_sTime)/CLOCKS_PER_SEC<<"s\n"; 
	/* Print the indirect call result in json format */

	/* Print the information of structure map. */
#ifdef _PRINT_STMAP
	OP << "===>structMap size:" << GlobalCtx.structAnalyzer.getSize() << "\n";
	OP << "structMap:\n";
	GlobalCtx.structAnalyzer.printStructInfo();
#endif
#ifdef CG_DEBUG
	/* Print the CallMaps and CalledMaps. */
	OP << "Call graph:\n";
	for (auto i : GlobalCtx.CallMaps)
	{
		OP << i.first->getName().str() << " calls: ";
		for (Function *F : i.second)
		{
			OP << F->getName().str() << "/";
		}
		OP << "\n";
	}

	for (auto i : GlobalCtx.CalledMaps)
	{
		OP << i.first->getName().str() << " called by: ";
		for (Function *F : i.second)
		{
			OP << F->getName().str() << "/";
		}
		OP << "\n";
	}
	return 0;
#endif

	if (UbiIncAnalysis)
	{
		sTime=clock();
		/* Set the incremental analysis type. */
		int num = setUpIncFunctions(&GlobalCtx);
		OP << num << " functions initially: \n";
#ifdef _PRINT_MODIFIEDFUNCS
		for (auto item : GlobalCtx.modifiedFuncs)
		{
			OP << item->getName().str() << " in module " << GlobalCtx.ModuleMaps[item->getParent()] << "\n";
		}
		OP << "======================\n";
#endif
#ifdef CANDICG_DEBUG
		/* Print the CallMaps and CalledMaps. */
		OP << "candiCall graph:\n";
		for (auto i : GlobalCtx.candiCallMaps)
		{
			OP << i.first->getName().str() << " calls: ";
			for (Function *F : i.second)
			{
				OP << F->getName().str() << "/";
			}
			OP << "\n";
		}

		for (auto i : GlobalCtx.candiCalledMaps)
		{
			OP << i.first->getName().str() << " called by: ";
			for (Function *F : i.second)
			{
				OP << F->getName().str() << "/";
			}
			OP << "\n";
		}
#endif
		InitReadyFunction(&GlobalCtx);
		std::string oldDir = argv[argc - 2];
		std::string newDir = argv[argc - 1];

		QualifierAnalysis QAPass(&GlobalCtx);
		QAPass.runInc();

		OP << GlobalCtx.modifiedFuncs.size()<<" functions analyzed.\n";
		eTime=clock();
		errs()<<"==>Total Time for IncAnalysisYZ "<<(double)(eTime-sTime)/CLOCKS_PER_SEC<<"s\n";
#ifdef LOAD_NEWFILE
		/*unmap all the files in the previous analysis*/
		std::string oldBCList(argv[argc - 2]);
		oldBCList += +"/bitcode.list";
		OP<<"oldBCList = "<<oldBCList<<"\n";

		std::ifstream oldlist;
		oldlist.open(oldBCList);
		std::string bcstr;
		int fd;
		struct stat sb;
		char *mapped;
		while(oldlist>>bcstr) {
			std::unique_ptr<Module> M = NULL;

			if ((fd = open(bcstr.c_str(), O_RDWR)) < 0) {
				perror("open");
			}

			if ((fstat(fd, &sb)) == -1) {
				perror("fstat");
			}
			if ((mapped = (char *)mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0)) == (void*)-1) {
				perror("mmap") ;
			}
			if((munmap((void *)mapped,sb.st_size)) == -1) {
				OP<<bcstr.c_str()<<" not unmapped.\n";
			}
			//OP<<"sb.st_size = "<<sb.st_size<<"\n";
			close(fd);
		}

		/*Load the rest of the latest version files to the memory for next inc analysis*/
		for (unsigned i = 0; i < InputFilenames.size() - 2; ++i)
		{
			LLVMContext *LLVMCtx = new LLVMContext();
			std::string::size_type   pos(0);
			if((pos=InputFilenames[i].find(oldDir))!=std::string::npos)
				InputFilenames[i].replace(pos,oldDir.length(),newDir);

			StringRef MName = StringRef(strdup(InputFilenames[i].data()));
			std::unique_ptr<Module> M = NULL;

			int fd;
			struct stat sb;
			char *mapped;

			/*map all the new file to memory for next incremental analysis*/
			OP << "2load new file: [" << InputFilenames[i].c_str() << "]\n";
			M = parseIRFile(MName, Err, *LLVMCtx);
			if (M == NULL)
			{
				OP << argv[0] << ": error loading file '"
					<< MName.str() << "'\n";
				continue;
			}

			if ((fd = open(InputFilenames[i].c_str(), O_RDWR)) < 0)
			{
				perror("open");
			}

			if ((fstat(fd, &sb)) == -1)
			{
				perror("fstat");
			}

			if ((mapped = (char *)mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void *)-1)
			{
				perror("mmap");
				OP << InputFilenames[i].c_str() << " not mapped.\n";
			}
			close(fd);
		}

#endif
	}

	/* Non-incremental qualifier analysis for all functions. */
	if (UniAnalysis)
	{
		for (auto i : GlobalCtx.CallMaps)
		{
			GlobalCtx.RemainedFunction[i.first] = i.second.size();
		}
		InitReadyFunction(&GlobalCtx);
		OP << GlobalCtx.Visit.size() << " functions in total.\n";
		QualifierAnalysis QAPass(&GlobalCtx);
		QAPass.run();
		//QAPass.run(GlobalCtx.Modules);
	}
	totaleTime = clock();
	errs()<<"==>Total Analysis Time "<<(double)(totaleTime-totalsTime)/CLOCKS_PER_SEC<<"s\n";
#ifdef REST_F
	std::set<Function *> fset;
	unsigned remained = 0;
	for (auto ml : GlobalCtx.Modules)
	{
		for (Function &f : *ml.first)
		{
			Function *F = &f;
			if (GlobalCtx.Visit.find(F) != GlobalCtx.Visit.end() && (!GlobalCtx.Visit[F]))
			{
				fset.insert(F);
				if (GlobalCtx.incAnalysis)
				{
					OP << "@Function " << F << " :" << F->getName().str() << " owns " << GlobalCtx.incFuncRemained[F] << " callees remained.\n";
					if (GlobalCtx.incCandiFuncs[F])
					{
						OP << " needs analysis.\n";
					}
					int count = 1;
					for (auto cf : GlobalCtx.candiCallMaps[F])
					{
						//OP<<"cf: "<<cf->getName().str()<<"\n";
						if (!GlobalCtx.Visit[cf] && GlobalCtx.incCandiFuncs[cf])
						{
							OP << "--" << count++ << ". " << cf << ": " << cf->getName().str();
							if (cf->empty())
								OP << " : Empty.";
							if (GlobalCtx.incCandiFuncs[cf])
							{
								OP << " : true.\n";
							}
							else
							{
								OP << " : false.\n";
							}
							OP << "\n";
						}
					}
					int total_analyzed = 0;
					for (auto item : GlobalCtx.incCandiFuncs) {
						if (item.second) total_analyzed += 1;
					}
					OP<<"total_analyzed = "<<total_analyzed<<"\n";
				}
				else
				{
					OP << "@Function " << F << " :" << F->getName().str() << " owns " << GlobalCtx.RemainedFunction[F] << " callees remained.\n";
					int count = 1;
					for (auto cf : GlobalCtx.CallMaps[F])
					{
						OP<<"cf: "<<cf->getName().str()<<"\n";
						if (!GlobalCtx.Visit[cf])
						{
							OP << "--" << count++ << ". " << cf << ": " << cf->getName().str();
							if (cf->empty())
								OP << " : Empty.";
							OP << " : false.\n";
							OP << "\n";
						}
						else {
							OP<<"Not Empty.\n";
						}
					}
				}
			}
		}
	}

	OP << "# of remained function = " << fset.size() << "\n";
	OP<<" # of modified funcs = "<< GlobalCtx.modifiedFuncs.size()<<"\n";
#endif

	return 0;
}
