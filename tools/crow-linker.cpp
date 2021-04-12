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


static cl::opt<bool>
        Override("override", cl::desc("Override symbols"), cl::init(false));

static cl::opt<bool>
        InstrumentFunction("instrument-function", cl::desc("Instrument first function basic block to construct the call graph. When linking ensure that a function _cb(i: i32) exist"), cl::init(false));

static cl::opt<bool>
        InstrumentBB("instrument-bb", cl::desc("Instrument basic blocks to construct the call graph. When linking ensure that a function _cb(i: i32) exist"), cl::init(false));

static cl::opt<std::string> InstrumentCallbackName("callback-function-name",
                                      cl::desc("Callback function name for callgraph instrumentation")
        , cl::init("_cb71P5H47J3A"));

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
        errs() << "Constructing call" << "\n";


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
    Function * fCb = nullptr;
    if(InstrumentFunction){
        fCb = declare_function_instrument_cb(*bitcode, context);
    }

    // Set override flag
    unsigned Flags = Linker::Flags::None;

    if(Override)
        Flags |=  Linker::Flags::OverrideFromSrc;


    for(auto &module: BCFiles) {

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
        for(auto &fname : FunctionNames){

            if(DebugLevel > 1)
                errs() << "Parsing " << module << "\n";

            auto fObject = toMergeModule->getFunction(fname);

            if(fObject->isDeclaration())
                continue;

            if(!fObject){

                if(DebugLevel > 1)
                    errs() << "\tFunction " << fname << " does not exist\n";

                if(!SkipOnError){
                    llvm::report_fatal_error("\tFunction " + fname + " not found !");
                }

                continue; // continue since the function does not exist
            }

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

            // Check if the function has a special linkage
            if(backupLinkage4Functions.count(fObject->getName().str())) // Set the nw function type as the original
                backupLinkage4Functions[newName] = backupLinkage4Functions[fObject->getName().str()];

            // Change function name
            fObject->setName(newName);

            if(DebugLevel > 2)
                errs() << "Ready to merge " << newNameOutput.str() << "\n";

            outs() << newName << "\n";
            added++;
        }

        linker.linkInModule(std::move(toMergeModule), Flags);

        modulesCount++;
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
