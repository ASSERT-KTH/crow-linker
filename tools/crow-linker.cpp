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


static cl::opt<unsigned> DebugLevel(
        "crow-merge-debug-level",
        cl::desc("Pass devbug level, 0 for none"),
        cl::init(0));

static unsigned modulesCount = 0;

inline bool exists (const std::string& name) {
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

int main(int argc, const char **argv) {

    cl::ParseCommandLineOptions(argc, argv);

    LLVMContext context;
    SMDiagnostic error;

    auto bitcode = parseIRFile(InputFilename, error, context);

    Linker linker(*bitcode);

    errs() << "modules count " << BCFiles.size() << "\n";
    errs() << "function count " << FunctionNames.size() << "\n";

    for(auto &module: BCFiles) {

        if (!exists(module)) {
            errs() << "Module " << module << " does not exist\n";

            if (!SkipOnError) {
                llvm::report_fatal_error("Module " + module + " not found !");
            }

            continue; // continue since the module does not exist
        }

        auto toMergeModule = parseIRFile(InputFilename, error, context);

        for(auto &fname : FunctionNames){


            errs() << "Parsing " << module << "\n";

            auto fObject = toMergeModule->getFunction(fname);

            if(!fObject){
                errs() << "Function " << fname << " does not exist\n";

                if(!SkipOnError){
                    llvm::report_fatal_error("Function " + fname + " not found !");
                }

                continue; // continue since the function does not exist
            }

            errs() << "Merging function " << fname << "\n";


            // Change function name
        }

        modulesCount++;
    }

    std::error_code EC;
    llvm::raw_fd_ostream OS(OutFileName, EC, llvm::sys::fs::F_None);
    WriteBitcodeToFile(*bitcode, OS);
    OS.flush();

    return 0;
}
