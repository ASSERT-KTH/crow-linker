// Copyright 2014 The Souper Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "llvm/ADT/Twine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include <system_error>
#include "llvm/Linker/Linker.h"
#include <llvm/IRReader/IRReader.h>
#include <sys/stat.h>
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<original bitcode>"), cl::init("-"));

static cl::opt<std::string> OutFileName(cl::Positional, cl::desc("<result bitcode>"));

static cl::list<std::string> BCFiles("crow-merge-bitcodes",
                                     cl::desc("<variant file>")
        , cl::OneOrMore, cl::CommaSeparated);


static cl::list<std::string> FunctionNames("crow-merge-functions",
                                           cl::desc("<function name>")
        , cl::OneOrMore, cl::CommaSeparated);

static cl::opt<std::string> FuncSufix("crow-merge-func-sufix",
                                      cl::desc("Name sufix in the added functions")
        , cl::init("_"));

static cl::opt<bool> SkipOnError(
        "crow-merge-skip-on-error",
        cl::desc("Skip the merge of a module if error"),
        cl::init(false));


static cl::opt<bool> NotLookForFunctions(
        "skip-function-names",
        cl::desc("Skip function names"),
        cl::init(false));


static cl::opt<bool>
        Override("override", cl::desc("Override symbols"), cl::init(false));

static cl::opt<bool>
        InstrumentFunction("instrument-function", cl::desc("Instrument first function basic block to construct the call graph. When linking ensure that a function _cb(i: i32) exist"), cl::init(false));

static cl::opt<bool>
        InstrumentBB("instrument-bb", cl::desc("Instrument basic blocks to construct the call graph. When linking ensure that a function _cb(i: i32) exist"), cl::init(false));

static cl::opt<std::string> InstrumentCallbackName("callback-function-name",
                                      cl::desc("Callback function name for callgraph instrumentation")
        , cl::init("_cb71P5H47J3A"));

static cl::opt<std::string> DiscrminatorCallbackName("discrminator-callback-function-name",
                                                   cl::desc("Callback function name to get the discriminator")
        , cl::init("discriminate"));


static cl::opt<std::string> MergeFunctionSuffix("merge-function-suffix",
                                                   cl::desc("N1 diversifier function suffix")
        , cl::init("_n1"));


static cl::opt<bool> MergeFunctionAsPtr("merge-function-ptrs",
                                                cl::desc("Create the discrminator based on a global array of ptrs, accessing the function by the index in the array. This will create a Wasm table alter if the target is wasm32")
        , cl::init(false));

static cl::opt<bool> MergeFunctionAsSCases("merge-function-switch-cases",
                                        cl::desc("Create the discrminator based on a huge switch case.")
        , cl::init(false));

static cl::opt<bool> MergeAllAtOnce("merge-function-all-cases",
                                           cl::desc("Create the discrminator based on a huge switch case.")
        , cl::init(false));

static cl::opt<bool>
        InjectOnlyIfDifferent("merge-only-if-different", cl::desc("Add new function only if it is different to the original one"), cl::init(true));

static cl::opt<unsigned> DebugLevel(
        "crow-merge-debug-level",
        cl::desc("Pass devbug level, 0 for none"),
        cl::init(0));

static unsigned modulesCount = 0;

inline bool exists (const std::string& name) {
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

static std::map<std::string, GlobalValue::LinkageTypes> backupLinkage4Functions;
static std::map<std::string, GlobalValue::LinkageTypes> backupLinkage4Globals;

static std::map<Function*, std::vector<std::string>> origingalVariantsMap;

static void printVariantsMap(){
    for(auto &key: origingalVariantsMap){
        errs() << key.first->getName() << ": " << key.second.size() + 1 << " " << &key << "\n";

        if (DebugLevel > 5){
            for(auto &v: origingalVariantsMap[key.first]){
                errs() << &v;
                //if(v->getName().str())
                errs() << " " << v;
                errs() << "\n";
            }
        }
    }
}

static void addVariant(Function *original, std::string &variantName){

    if(DebugLevel > 2)
        errs()  << "Adding new entry for " << original->getName() << " variant: " << variantName <<  "\n";

    if(!origingalVariantsMap.count(original)){
        // Create the entry
        std::vector<std::string> v;
        origingalVariantsMap[original] = v;
    }

    origingalVariantsMap[original].push_back(variantName);
}

static void deinternalize_module(Module &M, bool saveBackup=false){
    // For functions
    for(auto &F: M){
        if(saveBackup)
            backupLinkage4Functions[F.getName().str()] = F.getLinkage();
        F.setLinkage(GlobalValue::CommonLinkage);
    }

    // For globals
    for(auto &G: M.globals()){

        if(saveBackup)
            backupLinkage4Globals[G.getName().str()] = G.getLinkage();

        G.setLinkage(GlobalValue::CommonLinkage);
    }
}

static std::set<size_t> moduleFunctionHashes;
static std::hash<std::string> hasher;
static int instrumentId = 1000;

static bool is_same_func(std::string function_name, std::string module_file, bool saveIfNotIn=true){

    LLVMContext context;
    SMDiagnostic error;

    auto module = parseIRFile(module_file, error, context);

    auto fObject = module->getFunction(function_name);
    deinternalize_module(*module);

    std::string fObjectDump;
    llvm::raw_string_ostream fObjectDumpSS(fObjectDump);
    fObjectDumpSS << *fObject;

    size_t hash = hasher(fObjectDump);

    if(moduleFunctionHashes.count(hash)){ // Already exist
        return true;
    }

    if(DebugLevel > 1) {
        errs() << "Saving variant for " << function_name << " " << module_file << " hash " << hash << "\n";

        if(DebugLevel > 4){
            errs() << fObjectDump << "\n";
        }
    }


    moduleFunctionHashes.insert(hash);
    return false;
}

static void restore_linkage(Module &M){

    // For functions
    for(auto &F: M){
        if(backupLinkage4Functions.count(F.getName().str()))
            F.setLinkage(backupLinkage4Functions[F.getName().str()]);
    }

    // For globals
    for(auto &G: M.globals()){

        if(backupLinkage4Globals.count(G.getName().str()))
            G.setLinkage(backupLinkage4Globals[G.getName().str()]);
    }
}


Function* declare_function_discriminate(Module &M, LLVMContext &context){

    std::vector<Type*> args(0);
    FunctionType *tpe = FunctionType::get(Type::getInt32Ty(context), args,false);
    Function *callee = Function::Create(tpe, Function::ExternalLinkage, DiscrminatorCallbackName, M);

    return callee;
}

Function* declare_function_discriminator(Module &M, LLVMContext &context, Function& original, Function& discrminate, std::vector<std::string> &variants){

    std::string newName;
    llvm::raw_string_ostream newNameOutput(newName);
    newNameOutput << original.getName() << "_" << MergeFunctionSuffix;


    std::vector<Type*> args(0);
    FunctionType *tpe = original.getFunctionType();
    Function *callee = Function::Create(tpe, original.getLinkage(), newName, M);

    unsigned IDX = 0;
    auto originalArgs = original.args().begin();
    for(auto &arg: callee->args()){
        arg.setName(originalArgs[IDX++].getName());
    }

    // Debug only
    IRBuilder<> Builder(context);

    BasicBlock *BB = BasicBlock::Create(context, "entry", callee);
    Builder.SetInsertPoint(BB);

    std::vector<Value*> discriminatorArgs ;
    auto discriminateValue = Builder.CreateCall(&discrminate, discriminatorArgs, "");

    errs() << "Building the switch case " << original.getName() << " " << variants.size() << "\n";

    //
    std::vector<Value*> Values;
    for (auto &Arg : callee->args()) {
        Values.push_back(&Arg);
    }


    errs() << "Finishing the switch case" << "\n";


    std::vector<BasicBlock*> bbs;
    for(auto &variant: variants) {


        std::string bbName;
        llvm::raw_string_ostream bbNameOutput(bbName);
        bbNameOutput << "case_" << variant;

        errs() << "Variant case " << bbName << "\n";

        BasicBlock *caseBB = BasicBlock::Create(context,bbName, callee);

        //auto bbval = Builder.CreateCall(&original, Values, "");
        //Builder.CreateRet(bbval);
        //auto *bid = llvm::ConstantInt::get(Type::getInt32Ty(BB->getContext()), instrumentId++);
        bbs.push_back(caseBB);
    }

    errs() << "BB created" << "\n";

    BasicBlock *EndBB = BasicBlock::Create(context, "end", callee);
    auto phi = Builder.CreateSwitch(discriminateValue, EndBB, variants.size());
    //Builder.SetInsertPoint(EndBB);

    IDX=0;
    for(auto &variant: variants) {

        auto func = M.getFunction(variant);

        auto *bid = llvm::ConstantInt::get(Type::getInt32Ty(context), IDX);
        phi->addCase(bid, bbs[IDX++]);
        Builder.CreateRet(
                Builder.CreateCall(func, Values, "")
        );
    }

    errs() << "BB bodies created" << "\n";
    //Builder.SetInsertPoint(EndBB);

    Builder.CreateRet(
            Builder.CreateCall(&original, Values, "")
    );
    errs() << "Returning the switch case" << "\n";

    return callee;
}


Function* declare_function_instrument_cb(Module &M, LLVMContext &context){

    std::vector<Type*> args(1,
                            Type::getInt32Ty(context));
    FunctionType *tpe = FunctionType::get(Type::getVoidTy(context), args,false);
    Function *callee = Function::Create(tpe, Function::ExternalLinkage, InstrumentCallbackName, M);

    return callee;
}

void instrument_BB(BasicBlock *BB, Function *fCb){


    // Construct call
    IRBuilder builder(BB);
    if (DebugLevel > 2)
        errs() << "Constructing call for " << BB->getParent()->getName() << " isDeclaration: " << BB->getParent()->isDeclaration()  << "\n";


    BasicBlock::iterator insertIn = BB->getFirstInsertionPt();
    while (isa<AllocaInst>(insertIn))  ++insertIn;

    Value *bid = llvm::ConstantInt::get(Type::getInt32Ty(BB->getContext()), instrumentId++);
    CallInst::Create(fCb, bid, "", cast<Instruction>(insertIn));

    if (DebugLevel > 2)
        errs() << "Inserting before" << *insertIn << "\n";
}

int main(int argc, const char **argv) {

    // General stats
    unsigned added = 0;

    cl::ParseCommandLineOptions(argc, argv);

    LLVMContext context;
    SMDiagnostic error;

    auto bitcode = parseIRFile(InputFilename, error, context);

    Linker linker(*bitcode);

    if(DebugLevel > 1) {
        errs() << "Modules count " << BCFiles.size() << "\n";
        errs() << "Function count " << FunctionNames.size() << "\n";
    }
    // Deinternalize functions
    deinternalize_module(*bitcode, /*save linkage as backup*/ true);

    //
    if(DebugLevel > 1) {
        errs() << "Hashing original functions" << BCFiles.size() << "\n";
    }

    for(auto &F: *bitcode){
        is_same_func(F.getName().str(), InputFilename);
    }

    // Declare _cb71P5H47J3A(i: i32) -> void
    if(DebugLevel > 2)
        errs() << "Injecting instrumentation callback\n";

    Function * fCb = nullptr;
    if(InstrumentFunction){
        fCb = declare_function_instrument_cb(*bitcode, context);
    }

    // Set override flag
    unsigned Flags = Linker::Flags::None;

    if(Override)
        Flags |=  Linker::Flags::OverrideFromSrc;


    if(DebugLevel > 2)
        errs() << "Merging modules\n";

    for(auto &module: BCFiles) {

        if(DebugLevel > 3)
            errs() << "Merging module " << module << "\n";

        if (!exists(module)) {

            if(DebugLevel > 1)
                errs() << "Module " << module << " does not exist\n";

            if (!SkipOnError) {
                llvm::report_fatal_error("Module " + module + " not found !");
            }

            continue; // continue since the module does not exist
        }

        auto toMergeModule = parseIRFile(module, error, context);

        deinternalize_module(*toMergeModule);
        if(!NotLookForFunctions)
            for(auto &fname : FunctionNames){

                if(DebugLevel > 1)
                    errs() << "Adding function " << fname << "\n";

                if(fname.empty())
                    continue;

                auto *fObject = toMergeModule->getFunction(fname);

                if(fObject->isDeclaration())
                    continue;

                if(DebugLevel > 1)
                    errs() << "\tMerging function " << fname << "\n";

                if(InjectOnlyIfDifferent){

                    if(DebugLevel > 1)
                        errs() << "\t Cheking for identical function" << "\n";

                    if(is_same_func(fname, module)){

                        if(DebugLevel > 0)
                            errs() << "\t Removing identical function " << fname << " in " << module << "\n";

                        continue;
                    }

                }

                std::string newName;
                llvm::raw_string_ostream newNameOutput(newName);
                newNameOutput << fname << "_" << modulesCount << FuncSufix;
                newNameOutput.flush();


                // Check if the function has a special linkage
                if(backupLinkage4Functions.count(fObject->getName().str())) // Set the nw function type as the original
                    backupLinkage4Functions[newName] = backupLinkage4Functions[fObject->getName().str()];

                // Change function name
                fObject->setName(newNameOutput.str());

                if(DebugLevel > 2)
                    errs() << "Ready to merge " << newNameOutput.str() << "\n";

                outs() << newNameOutput.str() << "\n";
                added++;

                auto original = bitcode->getFunction(fname);

                if(original){
                    addVariant(original, newNameOutput.str());
                }
                else{
                    errs() << "WARNING: " << "original bitcode does not contain the function " << fname << "\n";
                }
            }

        linker.linkInModule(std::move(toMergeModule), Flags);

        modulesCount++;
    }

    if(MergeFunctionAsPtr || MergeFunctionAsSCases){

        if(DebugLevel > 4){
            // print all map etry count
            printVariantsMap();
        }
        // Register discrminator function as external
        if(DebugLevel > 2){
            // print all map etry count
            errs() << "Defining discrminate function " << "\n";
        }
        auto discriminate = declare_function_discriminate(*bitcode, context);

        // for each function in the map
        for(auto &kv: origingalVariantsMap){
            auto mergeFunction = declare_function_discriminator(*bitcode, context, *kv.first, *discriminate, kv.second);
            errs()  << mergeFunction->getName() << "\n";
        }


    }
    // Restore initial function and global linkage
    restore_linkage(*bitcode);

    // Instrument for callgraph if needed
    if((InstrumentFunction || InstrumentBB) && fCb){

        if(DebugLevel > 2)
            errs() << "Instrumenting functions for callgraph"  << "\n";

        for(auto &F: *bitcode){
            if(!F.isDeclaration()) {

                if (DebugLevel > 2)
                    errs() << F.getName() << "Instrumenting basic blocks" << "\n";

                for(auto &BBA: F){
                    // Instrument all BB
                    instrument_BB(&BBA, fCb);

                    // TODO, print map for future analysis
                    errs() << F.getName() << ", " << instrumentId << "\n";

                    if(!InstrumentBB && InstrumentFunction){
                        break;
                    }
                }

            }
        }

    }

    std::error_code EC;
    llvm::raw_fd_ostream OS(OutFileName, EC, llvm::sys::fs::F_None);
    WriteBitcodeToFile(*bitcode, OS);
    OS.flush();

    errs() << "Added functions " << added << "\n";

    return 0;
}
