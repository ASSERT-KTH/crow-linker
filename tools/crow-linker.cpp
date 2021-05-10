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
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "instrumentor/Instrumentor.h"
#include "discriminator/Discriminator.h"


using namespace llvm;
using namespace crow_linker;

unsigned DebugLevel;

/* Discrminator options and flags */
extern std::string MergeFunctionSuffix;
extern std::string DiscrminatorCallbackName;
extern bool MergeFunctionAsPtr;
extern bool MergeFunctionAsSCases;

/* Instrumentor options */
extern bool InstrumentFunction;
extern bool InstrumentBB;

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
        InjectOnlyIfDifferent("merge-only-if-different", cl::desc("Add new function only if it is different to the original one"), cl::init(true));

static cl::opt<unsigned, true> DebugLevelFlag(
        "crow-merge-debug-level",
        cl::desc("Pass devbug level, 0 for none"),
        cl::location(DebugLevel),
        cl::init(0));

static unsigned modulesCount = 0;


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


static void addVariant(std::string &original, std::string &variantName){

    if(DebugLevel > 2)
        errs()  << "Adding new entry for " << original << " variant: " << variantName <<  "\n";

    if(origingalVariantsMap.count(original) == 0){
        // Create the entry
        std::vector<std::string> v;
        origingalVariantsMap[original] = v;
    }

    origingalVariantsMap[original].push_back(variantName);
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
        // Save original functions and globals
        is_same_func(F.getName().str(), InputFilename);
    }

    // Declare _cb71P5H47J3A(i: i32) -> void
    if(DebugLevel > 2)
        errs() << "Injecting instrumentation callback\n";

    Function * fCb = nullptr;
    if(InstrumentFunction){
        fCb =  declare_function_instrument_cb(*bitcode, context);
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
                    errs() << "Adding function " << fname << " Module " << module << "\n";

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
                    addVariant(fname
                               , newNameOutput.str());
                }
                else{
                    errs() << "WARNING: " << "original bitcode does not contain the function " << fname << "\n";
                }
            }

        linker.linkInModule(std::move(toMergeModule), Flags);

        modulesCount++;
    }


    crow_linker::merge_variants(*bitcode, context, origingalVariantsMap);
    // Restore initial function and global linkage
    restore_linkage(*bitcode);

    crow_linker::instrument_functions(fCb, *bitcode);

    std::error_code EC;
    llvm::raw_fd_ostream OS(OutFileName, EC, llvm::sys::fs::F_None);
    WriteBitcodeToFile(*bitcode, OS);
    OS.flush();

    errs() << "Added functions " << added << "\n";

    return 0;
}
